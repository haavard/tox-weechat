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
#include <stdio.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"

#include "tox-weechat-utils.h"

void
tox_weechat_hex2bin(const char *hex, size_t length, char *out)
{
    const char *position = hex;

    for (size_t i = 0; i < length / 2; ++i)
    {
        sscanf(position, "%2hhx", &out[i]);
        position += 2;
    }
}

void
tox_weechat_bin2hex(const uint8_t *bin, size_t size, char *out)
{
    char *position = out;

    for (size_t i = 0; i < size; ++i)
    {
        sprintf(position, "%02X", bin[i]);
        position += 2;
    }
    *position = 0;
}

char *
tox_weechat_null_terminate(const uint8_t *str, size_t length)
{
    char *str_null = malloc(length + 1);
    memcpy(str_null, str, length);
    str_null[length] = 0;

    return str_null;
}

char *
tox_weechat_get_name_nt(Tox *tox, int32_t friend_number)
{
    size_t length = tox_get_name_size(tox, friend_number);
    uint8_t name[length];

    // if no name, return client ID instead
    if (!length)
    {
        uint8_t client_id[TOX_CLIENT_ID_SIZE];
        tox_get_client_id(tox, friend_number, client_id);

        char *hex = malloc(TOX_CLIENT_ID_SIZE * 2 + 1);
        tox_weechat_bin2hex(client_id, TOX_CLIENT_ID_SIZE, hex);

        return hex;
    }

    tox_get_name(tox, friend_number, name);
    return tox_weechat_null_terminate(name, length);
}

char *
tox_weechat_get_status_message_nt(Tox *tox, int32_t friend_number)
{
    size_t length = tox_get_status_message_size(tox, friend_number);
    uint8_t message[length];
    tox_get_status_message(tox, friend_number, message, length);

    return tox_weechat_null_terminate(message, length);
}

char *
tox_weechat_get_self_name_nt(Tox *tox)
{
    size_t length = tox_get_self_name_size(tox);
    uint8_t name[length];
    tox_get_self_name(tox, name);

    return tox_weechat_null_terminate(name, length);
}
