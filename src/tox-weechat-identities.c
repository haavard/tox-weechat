/*
 * Copyright (c) 2014 Håvard Pettersson <haavard.pettersson@gmail.com>
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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <stdbool.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"
#include "tox-weechat-config.h"
#include "tox-weechat-friend-requests.h"
#include "tox-weechat-chats.h"
#include "tox-weechat-tox-callbacks.h"
#include "tox-weechat-utils.h"

#include "tox-weechat-identities.h"

struct t_tox_weechat_identity *tox_weechat_identities = NULL;
struct t_tox_weechat_identity *tox_weechat_last_identity = NULL;

char *tox_weechat_bootstrap_addresses[] = {
    "192.254.75.98",
    "31.7.57.236",
    "107.161.17.51",
    "144.76.60.215",
    "23.226.230.47",
    "37.59.102.176",
    "37.187.46.132",
    "178.21.112.187",
    "192.210.149.121",
    "54.199.139.199",
    "63.165.243.15",
};

uint16_t tox_weechat_bootstrap_ports[] = {
    33445, 443, 33445, 33445, 33445,
    33445, 33445, 33445, 33445, 33445,
    443,
};

char *tox_weechat_bootstrap_keys[] = {
    "951C88B7E75C867418ACDB5D273821372BB5BD652740BCDF623A4FA293E75D2F",
    "2A4B50D1D525DA2E669592A20C327B5FAD6C7E5962DC69296F9FEC77C4436E4E",
    "7BE3951B97CA4B9ECDDA768E8C52BA19E9E2690AB584787BF4C90E04DBB75111",
    "04119E835DF3E78BACF0F84235B300546AF8B936F035185E2A8E9E0A67C8924F",
    "A09162D68618E742FFBCA1C2C70385E6679604B2D80EA6E84AD0996A1AC8A074",
    "B98A2CEAA6C6A2FADC2C3632D284318B60FE5375CCB41EFA081AB67F500C1B0B",
    "5EB67C51D3FF5A9D528D242B669036ED2A30F8A60E674C45E7D43010CB2E1331",
    "4B2C19E924972CB9B57732FB172F8A8604DE13EEDA2A6234E348983344B23057",
    "F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038B9D822E8460FAB67",
    "7F9C31FE850E97CEFD4C4591DF93FC757C7C12549DDD55F8EEAECC34FE76C029",
    "8CD087E31C67568103E8C2A28653337E90E6B8EDA0D765D57C6B5172B4F1F04C",
};

int tox_weechat_bootstrap_count = sizeof(tox_weechat_bootstrap_addresses)
                                  / sizeof(tox_weechat_bootstrap_addresses[0]);

char *
tox_weechat_identity_data_file_path(struct t_tox_weechat_identity *identity)
{
    // expand path
    const char *weechat_dir = weechat_info_get ("weechat_dir", NULL);
    const char *base_path = weechat_config_string(identity->options[TOX_WEECHAT_IDENTITY_OPTION_SAVEFILE]);
    char *home_expanded = weechat_string_replace(base_path, "%h", weechat_dir);
    char *full_path = weechat_string_replace(home_expanded, "%n", identity->name);
    free(home_expanded);

    return full_path;
}

int
tox_weechat_load_identity_data_file(struct t_tox_weechat_identity *identity)
{
    char *full_path = tox_weechat_identity_data_file_path(identity);

    FILE *file = fopen(full_path, "r");
    free(full_path);

    if (file)
    {
        // get file size
        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        rewind(file);

        // allocate a buffer and read file into it
        uint8_t *data = malloc(sizeof(*data) * size);
        fread(data, sizeof(uint8_t), size, file);
        fclose(file);

        // try loading the data
        int status = tox_load(identity->tox, data, size);
        free(data);

        return status;
    }

    return -1;
}

int
tox_weechat_save_identity_data_file(struct t_tox_weechat_identity *identity)
{
    char *full_path = tox_weechat_identity_data_file_path(identity);

    char *rightmost_slash = strrchr(full_path, '/');
    char *save_dir = weechat_strndup(full_path, rightmost_slash - full_path);
    weechat_mkdir_parents(save_dir, 0755);
    free(save_dir);

    // save Tox data to a buffer
    uint32_t size = tox_size(identity->tox);
    uint8_t *data = malloc(sizeof(*data) * size);
    tox_save(identity->tox, data);

    // save buffer to a file
    FILE *file = fopen(full_path, "w");
    if (file)
    {
        fwrite(data, sizeof(data[0]), size, file);
        fclose(file);

        return 0;
    }

    return -1;
}

int
tox_weechat_identity_buffer_close_callback(void *data,
                                           struct t_gui_buffer *buffer)
{
    struct t_tox_weechat_identity *identity = data;
    identity->buffer = NULL;

    tox_weechat_identity_disconnect(identity);

    return WEECHAT_RC_OK;
}


int
tox_weechat_bootstrap_tox(Tox *tox, const char *address, uint16_t port, const char *public_key)
{
    char binary_key[TOX_FRIEND_ADDRESS_SIZE];
    tox_weechat_hex2bin(public_key, binary_key);

    int result = tox_bootstrap_from_address(tox,
                                            address,
                                            port,
                                            (uint8_t *)binary_key);

    return result;
}

void
tox_weechat_bootstrap_random_node(Tox *tox)
{
    int i = rand() % tox_weechat_bootstrap_count;
    tox_weechat_bootstrap_tox(tox, tox_weechat_bootstrap_addresses[i],
                                   tox_weechat_bootstrap_ports[i],
                                   tox_weechat_bootstrap_keys[i]);
}

struct t_tox_weechat_identity *
tox_weechat_identity_new(const char *name)
{
    struct t_tox_weechat_identity *identity = malloc(sizeof(*identity));
    identity->name = strdup(name);

    // add to identity list
    identity->prev_identity= tox_weechat_last_identity;
    identity->next_identity = NULL;

    if (tox_weechat_identities == NULL)
        tox_weechat_identities = identity;
    else
        tox_weechat_last_identity->next_identity = identity;

    tox_weechat_last_identity = identity;

    // set up internal vars
    identity->tox = NULL;
    identity->buffer = NULL;
    identity->tox_do_timer = NULL;
    identity->chats = identity->last_chat = NULL;
    identity->friend_requests = identity->last_friend_request = NULL;
    identity->tox_online = false;

    // set up config
    tox_weechat_config_init_identity(identity);

    return identity;
}

void
tox_weechat_identity_connect(struct t_tox_weechat_identity *identity)
{
    if (identity->tox)
        return;

    // create main buffer
    if (identity->buffer == NULL)
    {
        identity->buffer = weechat_buffer_new(identity->name,
                                              NULL, NULL,
                                              tox_weechat_identity_buffer_close_callback, identity);
    }

    weechat_printf(identity->buffer,
                   "%s%s: identity %s connecting",
                   weechat_prefix("network"),
                   weechat_plugin->name,
                   identity->name);

    // create Tox
    identity->tox = tox_new(NULL);

    // try loading Tox saved data
    if (tox_weechat_load_identity_data_file(identity) == -1)
    {
        // we failed to load - set an initial name
        char *name;

        struct passwd *user_pwd;
        if ((user_pwd = getpwuid(geteuid())))
            name = user_pwd->pw_name;
        else
            name = "Tox-WeeChat User";

        tox_set_name(identity->tox,
                     (uint8_t *)name, strlen(name));
    }

    // initialize friend requests
    tox_weechat_friend_request_init_identity(identity);

    if (identity->friend_request_count > 0)
        weechat_printf(identity->buffer,
                       "%sYou have %d pending friend requests.",
                       weechat_prefix("network"),
                       identity->friend_request_count);

    // bootstrap DHT
    int max_bootstrap_nodes = 5;
    int bootstrap_nodes = max_bootstrap_nodes > tox_weechat_bootstrap_count ?
                          tox_weechat_bootstrap_count : max_bootstrap_nodes;
    for (int i = 0; i < bootstrap_nodes; ++i)
        tox_weechat_bootstrap_random_node(identity->tox);

    // start Tox_do loop
    tox_weechat_do_timer_cb(identity, 0);

    // register Tox callbacks
    tox_callback_friend_message(identity->tox, tox_weechat_friend_message_callback, identity);
    tox_callback_friend_action(identity->tox, tox_weechat_friend_action_callback, identity);
    tox_callback_connection_status(identity->tox, tox_weechat_connection_status_callback, identity);
    tox_callback_name_change(identity->tox, tox_weechat_name_change_callback, identity);
    tox_callback_user_status(identity->tox, tox_weechat_user_status_callback, identity);
    tox_callback_status_message(identity->tox, tox_weechat_status_message_callback, identity);
    tox_callback_friend_request(identity->tox, tox_weechat_callback_friend_request, identity);
}

void
tox_weechat_identity_disconnect(struct t_tox_weechat_identity *identity)
{
    // check that we're not already disconnected
    if (!identity->tox)
        return;

    // save and kill tox
    int result = tox_weechat_save_identity_data_file(identity);
    tox_kill(identity->tox);
    identity->tox = NULL;

    if (result == -1)
    {
        char *path = tox_weechat_identity_data_file_path(identity);
        weechat_printf(NULL,
                       "%s%s: Could not save Tox identity %s to file: %s",
                       weechat_prefix("error"),
                       weechat_plugin->name,
                       identity->name,
                       path);
        free(path);
    }

    // stop Tox timer
    weechat_unhook(identity->tox_do_timer);

    // have to refresh and hide bar items even if we were already offline
    tox_weechat_identity_refresh_online_status(identity);
    tox_weechat_identity_set_online_status(identity, false);
}

void
tox_weechat_identity_autoconnect()
{
    for (struct t_tox_weechat_identity *identity = tox_weechat_identities;
         identity;
         identity = identity->next_identity)
    {
        if (weechat_config_boolean(identity->options[TOX_WEECHAT_IDENTITY_OPTION_AUTOCONNECT]))
            tox_weechat_identity_connect(identity);
    }
}

void
tox_weechat_identity_refresh_online_status(struct t_tox_weechat_identity *identity)
{
    weechat_bar_item_update("buffer_plugin");
    weechat_bar_item_update("input_prompt");
    weechat_bar_item_update("away");
}

void
tox_weechat_identity_set_online_status(struct t_tox_weechat_identity *identity,
                                       bool online)
{
    if (identity->tox_online ^ online)
    {
        identity->tox_online = online;
        tox_weechat_identity_refresh_online_status(identity);

        struct t_gui_buffer *buffer = identity->buffer ?: NULL;
        if (identity->tox_online)
        {
            weechat_printf(buffer,
                           "%s%s: identity %s connected",
                           weechat_prefix("network"),
                           weechat_plugin->name,
                           identity->name);
        }
        else
        {
            weechat_printf(buffer,
                           "%s%s: identity %s disconnected",
                           weechat_prefix("network"),
                           weechat_plugin->name,
                           identity->name);
        }
    }
}

struct t_tox_weechat_identity *
tox_weechat_identity_name_search(const char *name)
{
    for (struct t_tox_weechat_identity *identity = tox_weechat_identities;
         identity;
         identity = identity->next_identity)
    {
        if (weechat_strcasecmp(identity->name, name) == 0)
            return identity;
    }

    return NULL;
}

struct t_tox_weechat_identity *
tox_weechat_identity_for_buffer(struct t_gui_buffer *buffer)
{
    for (struct t_tox_weechat_identity *identity = tox_weechat_identities;
         identity;
         identity = identity->next_identity)
    {
        if (identity->buffer == buffer)
            return identity;

        for (struct t_tox_weechat_chat *chat = identity->chats;
             chat;
             chat = chat->next_chat)
        {
            if (chat->buffer == buffer)
                return identity;
        }
    }

    return NULL;
}

void
tox_weechat_identity_delete(struct t_tox_weechat_identity *identity,
                            bool delete_data)
{
    char *data_path = tox_weechat_identity_data_file_path(identity);

    tox_weechat_identity_free(identity);

    if (delete_data)
        unlink(data_path);
}

void
tox_weechat_identity_free(struct t_tox_weechat_identity *identity)
{
    // save friend requests
    tox_weechat_friend_request_save_identity(identity);
    tox_weechat_friend_request_free_identity(identity);

    // disconnect
    tox_weechat_identity_disconnect(identity);

    // close buffer
    if (identity->buffer)
    {
        weechat_buffer_set_pointer(identity->buffer, "close_callback", NULL);
        weechat_buffer_close(identity->buffer);
    }

    // remove from list
    if (identity == tox_weechat_last_identity)
        tox_weechat_last_identity = identity->prev_identity;

    if (identity->prev_identity)
        identity->prev_identity->next_identity = identity->next_identity;
    else
        tox_weechat_identities = identity->next_identity;

    if (identity->next_identity)
        identity->next_identity->prev_identity = identity->prev_identity;

    // free remaining vars
    free(identity->name);
    free(identity);
}

void
tox_weechat_identity_free_all()
{
    while (tox_weechat_identities)
        tox_weechat_identity_free(tox_weechat_identities);
}
