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

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>
#include <jansson.h>

#include "tox-weechat.h"
#include "tox-weechat-identities.h"
#include "tox-weechat-friend-requests.h"
#include "tox-weechat-utils.h"

#include "tox-weechat-json.h"

#define TOX_WEECHAT_JSON_CONFIG_PATH "%h/tox/data.json"

const char *tox_weechat_json_key_friend_requests = "friend_requests";
const char *tox_weechat_json_friend_request_key_client_id = "client_id";
const char *tox_weechat_json_friend_request_key_message = "message";

/**
 * Return the full path to the JSON data file.
 */
char *
tox_weechat_json_data_file_path()
{
    const char *weechat_dir = weechat_info_get("weechat_dir", NULL);
    return weechat_string_replace(TOX_WEECHAT_JSON_CONFIG_PATH, "%h", weechat_dir);
}

/**
 * Return the key used for an identity in the JSON data file. Must be freed.
 */
char *
tox_weechat_json_get_identity_key(struct t_tox_weechat_identity *identity)
{
    uint8_t address[TOX_FRIEND_ADDRESS_SIZE];
    tox_get_address(identity->tox, address);

    char *hex_id = malloc(TOX_CLIENT_ID_SIZE * 2 + 1);
    tox_weechat_bin2hex(address, TOX_CLIENT_ID_SIZE, hex_id);

    return hex_id;
}

/**
 * Save an identity's data to a JSON object.
 */
json_t *
tox_weechat_json_identity_save(struct t_tox_weechat_identity *identity)
{
    json_t *json_data = json_object();
    json_t *friend_request_array = json_array();
    if (!json_data || !friend_request_array)
        return NULL;

    for (struct t_tox_weechat_friend_request *request = identity->friend_requests;
         request;
         request = request->next_request)
    {
        char hex_id[TOX_CLIENT_ID_SIZE * 2 + 1];
        tox_weechat_bin2hex(request->tox_id, TOX_CLIENT_ID_SIZE, hex_id);

        json_t *json_request = json_object();
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

        json_array_append(friend_request_array, json_request);
        json_decref(json_request);
    }

    json_object_set(json_data,
                    tox_weechat_json_key_friend_requests,
                    friend_request_array);
    json_decref(friend_request_array);

    return json_data;
}

/**
 * Load an identity's data from a JSON object.
 */
void
tox_weechat_json_identity_load(struct t_tox_weechat_identity *identity,
                               json_t *data)
{
    json_t *friend_request_array = json_object_get(data, tox_weechat_json_key_friend_requests);

    if (friend_request_array)
    {
        tox_weechat_friend_request_free_identity(identity);

        size_t index;
        json_t *json_request;
        json_array_foreach(friend_request_array, index, json_request)
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
}

/**
 * Load the JSON data on disk into the in-memory identity objects.
 */
void
tox_weechat_json_load()
{
    char *full_path = tox_weechat_json_data_file_path();

    json_error_t error;
    json_t *json_data = json_load_file(full_path, 0, &error);
    free(full_path);

    if (json_data)
    {
        for (struct t_tox_weechat_identity *identity = tox_weechat_identities;
             identity;
             identity = identity->next_identity)
        {
            char *hex_id = tox_weechat_json_get_identity_key(identity);
            json_t *identity_data = json_object_get(json_data, hex_id);

            if (identity_data)
                tox_weechat_json_identity_load(identity, identity_data);
        }
        json_decref(json_data);
    }
}

/**
 * Save all in-memory data to JSON data on disk. Return 0 on success, -1 on
 * error.
 */
int
tox_weechat_json_save()
{
    json_t *json_data = json_object();
    if (!json_data)
        return -1;

    for (struct t_tox_weechat_identity *identity = tox_weechat_identities;
         identity;
         identity = identity->next_identity)
    {
        char *hex_id = tox_weechat_json_get_identity_key(identity);
        json_t *identity_data = tox_weechat_json_identity_save(identity);

        json_object_set(json_data, hex_id, identity_data);
        json_decref(identity_data);
    }

    char *full_path = tox_weechat_json_data_file_path();
    int rc = json_dump_file(json_data,
                            full_path,
                            0);
    free(full_path);
    json_decref(json_data);

    return rc;
}

