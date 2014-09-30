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

#include "twc.h"
#include "twc-list.h"
#include "twc-profile.h"
#include "twc-friend-request.h"
#include "twc-message-queue.h"
#include "twc-utils.h"

#include "twc-data.h"

// TODO: move to config
#define twc_JSON_CONFIG_PATH "%h/tox/data.json"

const char *twc_json_key_friend_requests = "freqs";
const char *twc_json_friend_request_key_tox_id = "tox_id";
const char *twc_json_friend_request_key_message = "msg";
const char *twc_json_key_message_queues = "msg_q";
const char *twc_json_key_message_queue_key_message = "msg";
const char *twc_json_key_message_queue_key_message_type = "type";
const char *twc_json_key_message_queue_key_message_timestamp = "time";

/**
 * The object holding the JSON config data.
 */
json_t *twc_json_data = NULL;

/**
 * Return the full path to the config file. Return value must be freed.
 */
char *
twc_data_json_path()
{
    const char *weechat_dir = weechat_info_get("weechat_dir", NULL);
    return weechat_string_replace(twc_JSON_CONFIG_PATH, "%h", weechat_dir);
}

/**
 * Return the key used for a profile in the JSON data file. Must be freed.
 */
char *
twc_data_profile_key(struct t_twc_profile *profile)
{
    uint8_t address[TOX_FRIEND_ADDRESS_SIZE];
    tox_get_address(profile->tox, address);

    char *hex_id = malloc(TOX_CLIENT_ID_SIZE * 2 + 1);
    twc_bin2hex(address, TOX_CLIENT_ID_SIZE, hex_id);

    return hex_id;
}

/**
 * Save an profile's friend requests to a json array. Returns a new
 * reference.
 */
json_t *
twc_data_save_profile_friend_requests(struct t_twc_profile *profile)
{
    json_t *friend_request_array = json_array();
    if (!friend_request_array)
        return NULL;

    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach(profile->friend_requests, index, item)
    {
        char hex_id[TOX_CLIENT_ID_SIZE * 2 + 1];
        twc_bin2hex(item->friend_request->tox_id, TOX_CLIENT_ID_SIZE, hex_id);

        json_t *json_request = json_object();
        json_t *json_id = json_string(hex_id);
        json_t *json_message = json_string(item->friend_request->message);
        if (!json_request || !json_id || !json_message)
            break;

        json_object_set_new(json_request,
                            twc_json_friend_request_key_tox_id,
                            json_id);

        json_object_set_new(json_request,
                            twc_json_friend_request_key_message,
                            json_message);

        json_array_append_new(friend_request_array, json_request);
    }

    return friend_request_array;
}

void
twc_data_save_profile_message_queues_map_callback(void *data,
                                                  struct t_hashtable *hashtable,
                                                  const void *key, const void *value)
{
    struct t_twc_list *message_queue = ((struct t_twc_list *)value);
    json_t *json_message_queues = data;

    json_t *json_message_queue = json_array();
    if (!json_message_queue)
        return;

    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach(message_queue, index, item)
    {
        struct t_twc_queued_message *queued_message = item->queued_message;

        json_t *message = json_string(queued_message->message);
        json_t *message_type = json_integer(queued_message->message_type);
        json_t *timestamp = json_integer(mktime(queued_message->time));
        json_t *message_object = json_object();

        if (!message || !message_type || !timestamp || !message_object)
            return;

        json_object_set_new(message_object,
                            twc_json_key_message_queue_key_message,
                            message);
        json_object_set_new(message_object,
                            twc_json_key_message_queue_key_message_type,
                            message_type);
        json_object_set_new(message_object,
                            twc_json_key_message_queue_key_message_timestamp,
                            timestamp);

        json_array_append_new(json_message_queue, message_object);
    }

    json_object_set_new(json_message_queues, key, json_message_queue);
}

/**
 * Save a profile's message queue to a json object. Returns a new reference.
 */
json_t *
twc_data_save_profile_message_queues(struct t_twc_profile *profile)
{
    json_t *message_queues = json_object();

    weechat_hashtable_map(profile->message_queues,
                          twc_data_save_profile_message_queues_map_callback,
                          message_queues);

    return message_queues;
}

/**
 * Save a profile's data.
 */
void
twc_data_save_profile(struct t_twc_profile *profile)
{
    json_t *json_data = json_object();
    if (!json_data)
        return;

    json_t *friend_requests = twc_data_save_profile_friend_requests(profile);
    json_object_set_new(json_data,
                        twc_json_key_friend_requests,
                        friend_requests);

    json_t *message_queues = twc_data_save_profile_message_queues(profile);
    json_object_set_new(json_data,
                        twc_json_key_message_queues,
                        message_queues);

    char *profile_key = twc_data_profile_key(profile);
    json_object_set_new(twc_json_data, profile_key, json_data);
    free(profile_key);
}

/**
 * Load friend requests from a json array.
 */
void
twc_data_load_profile_friend_requests(struct t_twc_profile *profile,
                                      json_t *friend_requests)
{
    twc_friend_request_free_profile(profile);

    size_t index;
    json_t *json_request;
    json_array_foreach(friend_requests, index, json_request)
    {
        char client_id[TOX_CLIENT_ID_SIZE];
        const char *message;

        json_t *json_id = json_object_get(json_request,
                                          twc_json_friend_request_key_tox_id);
        json_t *json_message = json_object_get(json_request,
                                               twc_json_friend_request_key_message);

        twc_hex2bin(json_string_value(json_id), TOX_CLIENT_ID_SIZE, client_id);
        message = json_string_value(json_message);

        twc_friend_request_add(profile,
                               (uint8_t *)client_id,
                               message);
    }
}

/**
 * Load message queues from a json object.
 */
void
twc_data_load_profile_message_queues(struct t_twc_profile *profile,
                                     json_t *message_queues)
{
    twc_message_queue_free_profile(profile);

    const char *key;
    json_t *message_queue;
    json_object_foreach(message_queues, key, message_queue)
    {
        
    }
}

/**
 * Load an profile's data from a JSON object.
 */
void
twc_data_profile_load(struct t_twc_identity *identity)
{
    char *profile_key = twc_json_get_identity_key(identity);
    json_t *profile_data = json_object_get(twc_json_data, identity_key);
    free(profile_key);

    json_t *friend_requests = json_object_get(profile_data,
                                              twc_json_key_friend_requests);
    if (friend_requests)
        twc_data_load_friend_requests(profile, friend_requests);

    json_t *unsent_messages = json_object_get(profile_data,
                                              twc_json_key_unsent_messages);
    if (unsent_messages)
        twc_data_load_unsent_messages(profile, unsent_messages);
}

/**
 * Load the data on disk into memory.
 */
void
twc_data_load()
{
    char *full_path = twc_json_data_file_path();


    json_error_t error;
    twc_json_data = json_load_file(full_path, 0, &error);
    free(full_path);

    if (!twc_json_data)
    {
        weechat_printf(NULL,
                       "%s%s: could not load on-disk data",
                       weechat_prefix("error"),
                       weechat_plugin->name);

        twc_json_data = json_object();
    }
}

/**
 * Save all in-memory data to data on disk. Return 0 on success, -1 on
 * error.
 */
int
twc_data_save()
{
    char *full_path = twc_json_data_file_path();
    int rc = json_dump_file(twc_json_data,
                            full_path,
                            0);
    free(full_path);

    return rc;
}

/**
 * Free in-memory JSON data.
 */
void
twc_data_free()
{
    json_decref(twc_json_data);
}

