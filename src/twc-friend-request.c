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
#include "twc-utils.h"

#include "twc-friend-request.h"

/**
 * Add a new friend request to a profile.
 *
 * Returns index on success, -1 on a full friend request list and -2 for any
 * other error.
 */
int
twc_friend_request_add(struct t_twc_profile *profile,
                       const uint8_t *client_id,
                       const char *message)
{
    size_t max_request_count =
        TWC_PROFILE_OPTION_INTEGER(profile, TWC_PROFILE_OPTION_MAX_FRIEND_REQUESTS);
    if (profile->friend_requests->count >= max_request_count)
        return -1;

    // create a new request
    struct t_twc_friend_request *request
        = malloc(sizeof(struct t_twc_friend_request));
    if (!request)
        return -2;

    request->profile = profile;
    request->message = strdup(message);
    memcpy(request->tox_id, client_id, TOX_PUBLIC_KEY_SIZE);

    if (!twc_list_item_new_data_add(profile->friend_requests, request))
        return -2;

    return profile->friend_requests->count - 1;
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
    twc_list_remove_with_data(request->profile->friend_requests, request);
}

/**
 * Get friend request with a given index.
 */
struct t_twc_friend_request *
twc_friend_request_with_index(struct t_twc_profile *profile, int64_t index)
{
    return twc_list_get(profile->friend_requests, index)->friend_request;
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

