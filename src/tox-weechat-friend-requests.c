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
#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>
#include <jansson.h>

#include "tox-weechat.h"
#include "tox-weechat-identities.h"
#include "tox-weechat-utils.h"
#include "tox-weechat-json.h"

#include "tox-weechat-friend-requests.h"

/**
 * Add a new friend request to an identity.
 *
 * Returns 0 on success, -1 on a full friend list.
 */
int
tox_weechat_friend_request_add(struct t_tox_weechat_identity *identity,
                               const uint8_t *client_id,
                               const char *message)
{
    // check friend request count
    struct t_config_option *option =
        identity->options[TOX_WEECHAT_IDENTITY_OPTION_MAX_FRIEND_REQUESTS];
    unsigned int max_requests = weechat_config_integer(option);

    if (identity->friend_request_count >= max_requests)
        return -1;

    struct t_tox_weechat_friend_request *request = malloc(sizeof(*request));
    request->identity = identity;
    request->message = strdup(message);
    memcpy(request->tox_id, client_id, TOX_CLIENT_ID_SIZE);

    // add to list
    request->prev_request = identity->last_friend_request;
    request->next_request = NULL;

    if (identity->friend_requests == NULL)
        identity->friend_requests = request;
    else
        identity->last_friend_request->next_request = request;

    identity->last_friend_request = request;
    ++(identity->friend_request_count);

    return 0;
}

/**
 * Remove and free a friend request from its identity.
 */
void
tox_weechat_friend_request_remove(struct t_tox_weechat_friend_request *request)
{
    struct t_tox_weechat_identity *identity = request->identity;
    if (request == identity->last_friend_request)
        identity->last_friend_request = request->prev_request;

    if (request->prev_request)
        request->prev_request->next_request = request->next_request;
    else
        identity->friend_requests = request->next_request;

    if (request->next_request)
        request->next_request->prev_request = request->prev_request;

    --(identity->friend_request_count);

    tox_weechat_friend_request_free(request);
}

/**
 * Accept a friend request. Removes and frees the request.
 */
void
tox_weechat_accept_friend_request(struct t_tox_weechat_friend_request *request)
{
    tox_add_friend_norequest(request->identity->tox, request->tox_id);
    tox_weechat_friend_request_remove(request);
}

/**
 * Return the friend request from the identity with the number num.
 */
struct t_tox_weechat_friend_request *
tox_weechat_friend_request_with_num(struct t_tox_weechat_identity *identity,
                                    unsigned int num)
{
    if (num >= identity->friend_request_count) return NULL;

    unsigned int i = 0;
    struct t_tox_weechat_friend_request *request = identity->friend_requests;
    while (i != num && request->next_request)
    {
        request = request->next_request;
        ++i;
    }

    return request;
}

/**
 * Remove all friend requests from an identity and add new ones from the JSON
 * array json_request_array.
 */
void
tox_weechat_friend_requests_load_json(struct t_tox_weechat_identity *identity,
                                      json_t *json_request_array)
{
    identity->friend_requests = identity->last_friend_request = NULL;
    identity->friend_request_count = 0;

    size_t index;
    json_t *json_request;
    json_array_foreach(json_request_array, index, json_request)
    {
        char client_id[TOX_CLIENT_ID_SIZE];
        const char *message;

        json_t *json_id = json_object_get(json_request,
                                          tox_weechat_json_friend_request_key_client_id);
        json_t *json_message = json_object_get(json_request,
                                               tox_weechat_json_friend_request_key_message);

        tox_weechat_hex2bin(json_string_value(json_id), TOX_CLIENT_ID_SIZE * 2, client_id);
        message = json_string_value(json_message);

        tox_weechat_friend_request_add(identity,
                                       (uint8_t *)client_id,
                                       message);
    }
}

/**
 * Save all friend requests for an identity to a json array. Returns NULL on
 * error.
 */
json_t *
tox_weechat_friend_requests_save_json(struct t_tox_weechat_identity *identity)
{
    json_t *friend_requests = json_array();
    if (!json_array)
        return NULL;

    for (struct t_tox_weechat_friend_request *request = identity->friend_requests;
         request;
         request = request->next_request)
    {
        json_t *json_request = json_object();

        char hex_id[TOX_CLIENT_ID_SIZE * 2 + 1];
        tox_weechat_bin2hex(request->tox_id, TOX_CLIENT_ID_SIZE, hex_id);

        json_t *json_id = json_string(hex_id);
        json_t *json_message = json_string(request->message);

        if (!json_request || !json_id || !json_message)
            break;

        json_object_set(json_request,
                        tox_weechat_json_friend_request_key_client_id,
                        json_id);
        json_decref(json_id);

        json_object_set(json_request,
                        tox_weechat_json_friend_request_key_message,
                        json_message);
        json_decref(json_message);

        json_array_append(friend_requests, json_request);
        json_decref(json_request);
    }

    return friend_requests;
}

/**
 * Free a friend request.
 */
void
tox_weechat_friend_request_free(struct t_tox_weechat_friend_request *request)
{
    free(request->message);
    free(request);
}

/**
 * Free all friend requests from an identity.
 */
void
tox_weechat_friend_request_free_identity(struct t_tox_weechat_identity *identity)
{
    while (identity->friend_requests)
        tox_weechat_friend_request_remove(identity->friend_requests);
}

