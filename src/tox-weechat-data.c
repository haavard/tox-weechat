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


/////// TODO //////////
// get rid of data from deleted/nonexisting identities so it doesn't accumulate
///////////////////////

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>
#include <jansson.h>

#include "tox-weechat.h"
#include "tox-weechat-identities.h"
#include "tox-weechat-friend-requests.h"
#include "tox-weechat-messages.h"
#include "tox-weechat-utils.h"

#include "tox-weechat-data.h"

#define TOX_WEECHAT_JSON_CONFIG_PATH "%h/tox/data.json"

const char *tox_weechat_json_key_friend_requests = "friend_requests";
const char *tox_weechat_json_friend_request_key_client_id = "client_id";
const char *tox_weechat_json_friend_request_key_message = "message";
const char *tox_weechat_json_key_unsent_messages = "unsent_messages";
const char *tox_weechat_json_unsent_messages_key_recipient_id = "recipient_id";

json_t *tox_weechat_json_data = NULL;

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
 * Save an identity's friend requests into a json array.
 */
json_t *
tox_weechat_data_friend_requests_json(struct t_tox_weechat_identity *identity)
{
    json_t *friend_request_array = json_array();
    if (!friend_request_array)
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

        json_object_set_new(json_request,
                            tox_weechat_json_friend_request_key_client_id,
                            json_id);

        json_object_set_new(json_request,
                            tox_weechat_json_friend_request_key_message,
                            json_message);

        json_array_append_new(friend_request_array, json_request);
    }

    return friend_request_array;
}

/**
 * Save an identity's unsent messages into a json object.
 */
json_t *
tox_weechat_data_unsent_messages_json(struct t_tox_weechat_identity *identity)
{
    json_t *messages_object = json_object();
    if (!messages_object)
        return NULL;

    for (struct t_tox_weechat_unsent_message_recipient *recipient = identity->unsent_message_recipients;
         recipient;
         recipient = recipient->next_recipient)
    {
        char hex_id[TOX_CLIENT_ID_SIZE * 2 + 1];
        tox_weechat_bin2hex(recipient->recipient_id, TOX_CLIENT_ID_SIZE, hex_id);

        json_t *json_messages = json_array();
        if (!json_messages)
            break;

        for (struct t_tox_weechat_unsent_message *message = recipient->unsent_messages;
             message;
             message = message->next_message)
        {
            json_t *json_message = json_string(message->message);
            if (!json_message)
                break;

            json_array_append_new(json_messages, json_message);
        }

        json_object_set_new(messages_object, hex_id, json_messages);
    }

    return messages_object;
}

/**
 * Save an identity's data.
 */
void
tox_weechat_data_identity_save(struct t_tox_weechat_identity *identity)
{
    json_t *json_data = json_object();
    if (!json_data)
        return;

    json_t *friend_requests = tox_weechat_data_friend_requests_json(identity);
    json_object_set_new(json_data,
                        tox_weechat_json_key_friend_requests,
                        friend_requests);

    json_t *unsent_messages = tox_weechat_data_unsent_messages_json(identity);
    json_object_set_new(json_data,
                        tox_weechat_json_key_unsent_messages,
                        unsent_messages);

    char *identity_key = tox_weechat_json_get_identity_key(identity);
    json_object_set_new(tox_weechat_json_data, identity_key, json_data);
    free(identity_key);
}

/**
 * Load friend requests from a json array.
 */
void
tox_weechat_data_load_friend_requests(struct t_tox_weechat_identity *identity,
                                      json_t *friend_requests)
{
    tox_weechat_friend_request_free_identity(identity);

    size_t index;
    json_t *json_request;
    json_array_foreach(friend_requests, index, json_request)
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
 * Load unsent messages from a json array.
 */
void
tox_weechat_data_load_unsent_messages(struct t_tox_weechat_identity *identity,
                                      json_t *friend_requests)
{
    // TODO
}

/**
 * Load an identity's data from a JSON object.
 */
void
tox_weechat_data_identity_load(struct t_tox_weechat_identity *identity)
{
    char *identity_key = tox_weechat_json_get_identity_key(identity);
    json_t *identity_data = json_object_get(tox_weechat_json_data, identity_key);
    free(identity_key);

    json_t *friend_requests = json_object_get(identity_data,
                                              tox_weechat_json_key_friend_requests);
    if (friend_requests)
        tox_weechat_data_load_friend_requests(identity, friend_requests);

    json_t *unsent_messages = json_object_get(identity_data,
                                              tox_weechat_json_key_unsent_messages);
    if (unsent_messages)
        tox_weechat_data_load_unsent_messages(identity, unsent_messages);
}

/**
 * Load the data on disk into memory.
 */
void
tox_weechat_data_load()
{
    char *full_path = tox_weechat_json_data_file_path();


    json_error_t error;
    tox_weechat_json_data = json_load_file(full_path, 0, &error);
    free(full_path);

    if (!tox_weechat_json_data)
    {
        weechat_printf(NULL,
                       "%s%s: could not load on-disk data",
                       weechat_prefix("error"),
                       weechat_plugin->name);

        tox_weechat_json_data = json_object();
    }
}

/**
 * Save all in-memory data to data on disk. Return 0 on success, -1 on
 * error.
 */
int
tox_weechat_data_save()
{
    char *full_path = tox_weechat_json_data_file_path();
    int rc = json_dump_file(tox_weechat_json_data,
                            full_path,
                            0);
    free(full_path);

    return rc;
}

/**
 * Free in-memory JSON data.
 */
void
tox_weechat_data_free()
{
    json_decref(tox_weechat_json_data);
}

