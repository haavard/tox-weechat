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

#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "twc.h"
#include "twc-list.h"
#include "twc-profile.h"
#include "twc-sqlite.h"
#include "twc-utils.h"

#include "twc-friend-request.h"

/**
 * Add a new friend request to a profile. Return it's index for accepting.
 *
 * Returns ID on success, -1 on a full friend request list, -2 on other error.
 */
int
twc_friend_request_add(struct t_twc_profile *profile,
                       const uint8_t *client_id,
                       const char *message)
{
    int max_request_count =
        TWC_PROFILE_OPTION_INTEGER(profile, TWC_PROFILE_OPTION_MAX_FRIEND_REQUESTS);
    int current_request_count = twc_sqlite_friend_request_count(profile);
    if (current_request_count >= max_request_count)
        return -1;

    // create a new request
    struct t_twc_friend_request *request
        = malloc(sizeof(struct t_twc_friend_request));
    if (!request)
        return -2;

    request->profile = profile;
    request->message = strdup(message);
    memcpy(request->tox_id, client_id, TOX_CLIENT_ID_SIZE);

    int rc = twc_sqlite_add_friend_request(profile, request);

    if (rc == -1)
        return -2;

    return rc;
}

/**
 * Accept a friend request. Remove and free the request.
 */
void
twc_friend_request_accept(struct t_twc_friend_request *request)
{
    tox_add_friend_norequest(request->profile->tox, request->tox_id);
    twc_friend_request_remove(request);
}

/**
 * Remove and free a friend request from its profile.
 */
void
twc_friend_request_remove(struct t_twc_friend_request *request)
{
    twc_sqlite_delete_friend_request_with_id(request->profile,
                                             request->request_id);
}

/**
 * Get friend request with a given index.
 */
struct t_twc_friend_request *
twc_friend_request_with_index(struct t_twc_profile *profile, int64_t index)
{
    return twc_sqlite_friend_request_with_id(profile, index);
}

/**
 * Free a friend request.
 */
void
twc_friend_request_free(struct t_twc_friend_request *request)
{
    free(request->message);
    free(request);
}

/**
 * Free all friend requests from a list.
 */
void
twc_friend_request_free_list(struct t_twc_list *list)
{
    struct t_twc_friend_request *request;
    while ((request = twc_list_pop(list)))
        twc_friend_request_free(request);

    free(list);
}

