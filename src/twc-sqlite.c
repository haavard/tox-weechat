/*
 * Copyright (c) 2015 Håvard Pettersson <mail@haavard.me>
 *
 * This file is part of Tox-WeeChat.
 *
 * Tox-WeeChat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Tox-WeeChat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tox-WeeChat.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>
#include <sqlite3.h>

#include "twc.h"
#include "twc-list.h"
#include "twc-profile.h"
#include "twc-friend-request.h"

#include "twc-sqlite.h"

sqlite3 *twc_sqlite_db = NULL;
struct t_twc_list *twc_sqlite_statements = NULL;

#ifdef TWC_DEBUG
#define TWC_SQLITE_DEBUG_RC(rc, expected_rc)                                  \
    if (rc != expected_rc)                                                    \
        weechat_printf(NULL,                                                  \
                       "%s%s: SQLite error in %s: error code %d",             \
                       weechat_prefix("error"), weechat_plugin->name,         \
                       __func__, rc);
#else
#define TWC_SQLITE_DEBUG_RC(rc, expected_rc) (void)rc;
#endif // TWC_DEBUG

/**
 * Create or reset an SQLite statement.
 */
#define TWC_SQLITE_STMT(statement, statement_str)                             \
    static sqlite3_stmt *statement = NULL;                                    \
    if (!statement)                                                           \
    {                                                                         \
        int rc = sqlite3_prepare_v2(twc_sqlite_db,                            \
                                    statement_str,                            \
                                    strlen(statement_str) + 1,                \
                                    &statement,                               \
                                    NULL);                                    \
        TWC_SQLITE_DEBUG_RC(rc, SQLITE_OK);                                   \
        if (rc != SQLITE_OK)                                                  \
            statement = NULL;                                                 \
        else                                                                  \
            twc_list_item_new_data_add(twc_sqlite_statements, statement);     \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        sqlite3_reset(statement);                                             \
    }

/**
 * Return the full path to our SQLite database file. Must be freed.
 */
char *
twc_sqlite_db_path()
{
    const char *weechat_dir = weechat_info_get("weechat_dir", NULL);
    return weechat_string_replace("%h/tox/data.db", "%h", weechat_dir);
}

/**
 * Initialize profile table. Return 0 on success, -1 on error.
 */
int
twc_sqlite_init_profiles()
{
    TWC_SQLITE_STMT(statement,
                    "CREATE TABLE IF NOT EXISTS profiles ("
                        "id INTEGER PRIMARY KEY,"
                        "tox_id BLOB UNIQUE NOT NULL"
                    ")");

    int rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE)
        return -1;
    else
        return 0;
}

/**
 * Initialize friend request table. Return 0 on success, -1 on error.
 */
int
twc_sqlite_init_friend_requests()
{
    TWC_SQLITE_STMT(statement,
                    "CREATE TABLE IF NOT EXISTS friend_requests ("
                        "id INTEGER PRIMARY KEY,"
                        "tox_id BLOB NOT NULL,"
                        "message TEXT,"
                        "profile_id INTEGER NOT NULL,"
                        "FOREIGN KEY(profile_id) REFERENCES profiles(id) ON DELETE CASCADE,"
                        "UNIQUE(tox_id, profile_id)"
                    ")");
    int rc = sqlite3_step(statement);
    TWC_SQLITE_DEBUG_RC(rc, SQLITE_DONE)
    if (rc != SQLITE_DONE)
        return -1;
    else
        return 0;
}

/**
 * Add a profile, if it does not exist.
 */
int
twc_sqlite_add_profile(struct t_twc_profile *profile)
{
    TWC_SQLITE_STMT(statement,
                    "INSERT OR IGNORE INTO profiles (tox_id)"
                    "VALUES (?)");

    uint8_t tox_id[TOX_FRIEND_ADDRESS_SIZE];
    tox_get_address(profile->tox, tox_id);
    sqlite3_bind_blob(statement, 1,
                      tox_id, TOX_CLIENT_ID_SIZE,
                      NULL);

    int rc = sqlite3_step(statement);
    TWC_SQLITE_DEBUG_RC(rc, SQLITE_DONE)
    if (rc != SQLITE_DONE)
        return -1;
    else
        return 0;
}

/**
 * Get the rowid of a profile. Returns true on success, false on error (e.g.
 * not found).
 */
bool
twc_sqlite_profile_id(struct t_twc_profile *profile, int64_t *out)
{
    if (!(profile->tox))
        return false;

    TWC_SQLITE_STMT(statement,
                    "SELECT id FROM profiles WHERE tox_id == ?");

    uint8_t tox_id[TOX_FRIEND_ADDRESS_SIZE];
    tox_get_address(profile->tox, tox_id);
    sqlite3_bind_blob(statement, 1,
                      tox_id, TOX_CLIENT_ID_SIZE,
                      NULL);

    int rc = sqlite3_step(statement);
    TWC_SQLITE_DEBUG_RC(rc, SQLITE_ROW)
    if (rc != SQLITE_ROW)
    {
        return false;
    }
    else
    {
        *out = sqlite3_column_int(statement, 0);
        return true;
    }
}

/**
 * Delete a profile from persistent storage.
 */
int
twc_sqlite_delete_profile(struct t_twc_profile *profile)
{
    int64_t profile_id;
    if (!twc_sqlite_profile_id(profile, &profile_id))
    {
        weechat_printf(NULL, "missing profile!");
        return -1;
    }

    TWC_SQLITE_STMT(statement,
                    "DELETE FROM profiles "
                    "WHERE id == ?");
    sqlite3_bind_int(statement, 1,
                     profile_id);

    int rc = sqlite3_step(statement);
    TWC_SQLITE_DEBUG_RC(rc, SQLITE_DONE)
    if (rc == SQLITE_DONE)
        return 0;
    else
        return -1;
}

/**
 * Add a friend request. Return ID on success, -1 on error.
 */
int
twc_sqlite_add_friend_request(struct t_twc_profile *profile,
                              struct t_twc_friend_request *friend_request)
{
    int64_t profile_id;
    if (!twc_sqlite_profile_id(profile, &profile_id))
    {
        weechat_printf(NULL, "missing profile!");
        return -1;
    }

    TWC_SQLITE_STMT(statement,
                    "INSERT OR REPLACE INTO friend_requests (tox_id, message, profile_id)"
                    "VALUES (?, ?, ?)");
    sqlite3_bind_blob(statement, 1,
                      friend_request->tox_id, TOX_CLIENT_ID_SIZE,
                      NULL);
    sqlite3_bind_text(statement, 2,
                      friend_request->message, strlen(friend_request->message) + 1,
                      NULL);
    sqlite3_bind_int(statement, 3,
                     profile_id);

    int rc = sqlite3_step(statement);
    TWC_SQLITE_DEBUG_RC(rc, SQLITE_DONE)
    if (rc != SQLITE_DONE)
        return -1;
    else
        return sqlite3_last_insert_rowid(twc_sqlite_db);
}

/**
 * Return the number of friend requests for a profile.
 */
int
twc_sqlite_friend_request_count(struct t_twc_profile *profile)
{
    int64_t profile_id;
    if (!twc_sqlite_profile_id(profile, &profile_id))
    {
        weechat_printf(NULL, "missing profile!");
        return -1;
    }

    TWC_SQLITE_STMT(statement,
                    "SELECT COUNT(*) FROM friend_requests WHERE profile_id == ?");
    sqlite3_bind_int(statement, 1,
                     profile_id);

    int rc = sqlite3_step(statement);
    TWC_SQLITE_DEBUG_RC(rc, SQLITE_ROW)
    if (rc != SQLITE_ROW)
        return 0;
    else
        return sqlite3_column_int(statement, 0);
}

/**
 * Convert a row from an SQLite statement to a friend request object.
 */
struct t_twc_friend_request *
twc_sqlite_friend_request_row(sqlite3_stmt *statement,
                              struct t_twc_profile *profile)
{
    struct t_twc_friend_request *request = malloc(sizeof(struct t_twc_friend_request));
    request->request_id = sqlite3_column_int(statement, 0);
    memcpy(request->tox_id,
           sqlite3_column_blob(statement, 1),
           TOX_CLIENT_ID_SIZE);
    request->message = strdup((const char *)sqlite3_column_text(statement, 2));
    request->profile = profile;

    return request;
}

/**
 * Return a list of all friend requests for the given profile.
 */
struct t_twc_list *
twc_sqlite_friend_requests(struct t_twc_profile *profile)
{
    int64_t profile_id;
    if (!twc_sqlite_profile_id(profile, &profile_id))
    {
        weechat_printf(NULL, "missing profile!");
        return NULL;
    }

    TWC_SQLITE_STMT(statement,
                    "SELECT id, tox_id, message "
                    "FROM friend_requests "
                    "WHERE profile_id == ?");
    sqlite3_bind_int(statement, 1,
                     profile_id);

    struct t_twc_list *friend_requests = twc_list_new();

    int rc;
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW)
    {
        struct t_twc_friend_request *request =
            twc_sqlite_friend_request_row(statement, profile);
        twc_list_item_new_data_add(friend_requests, request);
    }
    TWC_SQLITE_DEBUG_RC(rc, SQLITE_DONE)

    return friend_requests;
}

struct t_twc_friend_request *
twc_sqlite_friend_request_with_id(struct t_twc_profile *profile, int64_t id)
{
    int64_t profile_id;
    if (!twc_sqlite_profile_id(profile, &profile_id))
    {
        weechat_printf(NULL, "missing profile!");
        return NULL;
    }

    TWC_SQLITE_STMT(statement,
                    "SELECT id, tox_id, message "
                    "FROM friend_requests "
                    "WHERE id == ? AND profile_id == ?");
    sqlite3_bind_int(statement, 1, id);
    sqlite3_bind_int(statement, 2, profile_id);

    struct t_twc_friend_request *request;
    int rc = sqlite3_step(statement);
    if (rc == SQLITE_ROW)
    {
        request = twc_sqlite_friend_request_row(statement, profile);
        return request;
    }
    else
        TWC_SQLITE_DEBUG_RC(rc, SQLITE_DONE)

    return NULL;
}

int
twc_sqlite_delete_friend_request_with_id(struct t_twc_profile *profile,
                                         int64_t id)
{
    int64_t profile_id;
    if (!twc_sqlite_profile_id(profile, &profile_id))
    {
        weechat_printf(NULL, "missing profile!");
        return -1;
    }

    TWC_SQLITE_STMT(statement,
                    "DELETE FROM friend_requests "
                    "WHERE id == ? AND profile_id == ?");
    sqlite3_bind_int(statement, 1,
                     id);
    sqlite3_bind_int(statement, 2,
                     profile_id);

    int rc = sqlite3_step(statement);
    TWC_SQLITE_DEBUG_RC(rc, SQLITE_DONE)
    if (rc == SQLITE_DONE)
        return 0;
    else
        return -1;
}

/**
 * Initialize connection to SQLite database and create tables if necessary.
 * Returns 0 on success, -1 on failure.
 */
int
twc_sqlite_init()
{
    char *path = twc_sqlite_db_path();

    int rc = sqlite3_open(path, &twc_sqlite_db);
    free(path);

    TWC_SQLITE_DEBUG_RC(rc, SQLITE_OK)
    if (rc != SQLITE_OK)
    {
        weechat_printf(NULL,
                      "%s: could not open database: %s\n",
                      weechat_plugin->name,
                      sqlite3_errmsg(twc_sqlite_db));
        sqlite3_close(twc_sqlite_db);
        twc_sqlite_db = NULL;

        return -1;
    }

    // statement list (so we can finalize later)
    twc_sqlite_statements = twc_list_new();

    // initialize tables
    if (twc_sqlite_init_profiles() != 0 ||
        twc_sqlite_init_friend_requests() != 0)
        return -1;

    return 0;
}

/**
 * Close connection to SQLite database.
 */
void
twc_sqlite_end()
{
    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach(twc_sqlite_statements, index, item)
        sqlite3_finalize(item->data);

    int rc = sqlite3_close(twc_sqlite_db);
    TWC_SQLITE_DEBUG_RC(rc, SQLITE_OK)
}

