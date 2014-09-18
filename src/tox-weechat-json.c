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
#include "tox-weechat-utils.h"

#include "tox-weechat-json.h"

#define TOX_WEECHAT_JSON_CONFIG_PATH "%h/tox/data.json"

json_t *tox_weechat_json_config = NULL;

const char *tox_weechat_json_key_friend_requests = "friend_requests";
const char *tox_weechat_json_friend_request_key_client_id = "client_id";
const char *tox_weechat_json_friend_request_key_message = "message";

char *
tox_weechat_json_config_file_path()
{
    const char *weechat_dir = weechat_info_get("weechat_dir", NULL);
    return weechat_string_replace(TOX_WEECHAT_JSON_CONFIG_PATH, "%h", weechat_dir);
}

void
tox_weechat_json_init()
{
    char *full_path = tox_weechat_json_config_file_path();

    json_error_t error;
    tox_weechat_json_config = json_load_file(full_path,
                                             0,
                                             &error);
    free(full_path);

    if (!tox_weechat_json_config)
    {
        // TODO: read error
        tox_weechat_json_config = json_object();
    }
}

int
tox_weechat_json_save()
{
    char *full_path = tox_weechat_json_config_file_path();

    int rc = json_dump_file(tox_weechat_json_config,
                            full_path,
                            0);
    free(full_path);

    return rc;
}

json_t *
tox_weechat_json_get_identity_object(struct t_tox_weechat_identity *identity)
{
    if (!(identity->tox)) return NULL;

    uint8_t address[TOX_FRIEND_ADDRESS_SIZE];
    char hex_id[TOX_CLIENT_ID_SIZE * 2 + 1];
    tox_get_address(identity->tox, address);
    tox_weechat_bin2hex(address, TOX_CLIENT_ID_SIZE, hex_id);

    json_t *object = json_object_get(tox_weechat_json_config,
                                     hex_id);

    if (object == NULL)
    {
        object = json_object();
        json_object_set(tox_weechat_json_config, hex_id, object);
        json_decref(object);
    }

    return object;
}

int
tox_weechat_json_set_identity_object(struct t_tox_weechat_identity *identity,
                                     json_t *object)
{
    if (!(identity->tox)) return -1;

    uint8_t address[TOX_FRIEND_ADDRESS_SIZE];
    char hex_id[TOX_CLIENT_ID_SIZE * 2 + 1];
    tox_get_address(identity->tox, address);
    tox_weechat_bin2hex(address, TOX_CLIENT_ID_SIZE, hex_id);

    return json_object_set(tox_weechat_json_config,
                           hex_id, object);
}

