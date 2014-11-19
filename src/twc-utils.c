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

#include <string.h>
#include <stdio.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "twc.h"
#include "twc-config.h"

#include "twc-utils.h"

/**
 * Convert a hex string to it's binary equivalent of max size bytes.
 */
void
twc_hex2bin(const char *hex, size_t size, uint8_t *out)
{
    const char *position = hex;

    size_t i;
    for (i = 0; i < size; ++i)
    {
        sscanf(position, "%2hhx", &out[i]);
        position += 2;
    }
}

/**
 * Convert size bytes to a hex string. out must be at lesat size * 2 + 1
 * bytes.
 */
void
twc_bin2hex(const uint8_t *bin, size_t size, char *out)
{
    char *position = out;

    size_t i;
    for (i = 0; i < size; ++i)
    {
        sprintf(position, "%02X", bin[i]);
        position += 2;
    }
    *position = 0;
}

/**
 * Return a null-terminated copy of str. Must be freed.
 */
char *
twc_null_terminate(const uint8_t *str, size_t length)
{
    char *str_null = malloc(length + 1);
    memcpy(str_null, str, length);
    str_null[length] = 0;

    return str_null;
}

/**
 * Get the null-terminated name of a Tox friend. Must be freed.
 */
char *
twc_get_name_nt(Tox *tox, int32_t friend_number)
{
    size_t length = tox_get_name_size(tox, friend_number);
    uint8_t name[length];

    // if no name, return client ID instead
    if (!length)
        return twc_get_friend_id_short(tox, friend_number);

    tox_get_name(tox, friend_number, name);
    return twc_null_terminate(name, length);
}

/**
 * Return the null-terminated status message of a Tox friend. Must be freed.
 */
char *
twc_get_status_message_nt(Tox *tox, int32_t friend_number)
{
    size_t length = tox_get_status_message_size(tox, friend_number);
    uint8_t message[length];
    tox_get_status_message(tox, friend_number, message, length);

    return twc_null_terminate(message, length);
}

/**
 * Return the name of a group chat peer as a null terminated string. Must be
 * freed.
 */
char *
twc_get_peer_name_nt(Tox *tox, int32_t group_number, int32_t peer_number)
{
    uint8_t name[TOX_MAX_NAME_LENGTH] = {0};

    int length = tox_group_peername(tox, group_number, peer_number, name);
    if (length >= 0)
        return twc_null_terminate(name, length);
    else
        return "<unknown>";
}

/**
 * Return the users own name, null-terminated. Must be freed.
 */
char *
twc_get_self_name_nt(Tox *tox)
{
    size_t length = tox_get_self_name_size(tox);
    uint8_t name[length];
    tox_get_self_name(tox, name);

    return twc_null_terminate(name, length);
}

/**
 * Return a friend's Tox ID in short form. Return value must be freed.
 */
char *
twc_get_friend_id_short(Tox *tox, int32_t friend_number)
{
    uint8_t client_id[TOX_CLIENT_ID_SIZE];
    tox_get_client_id(tox, friend_number, client_id);

    size_t short_id_length = weechat_config_integer(twc_config_short_id_size);

    char *hex_address = malloc(short_id_length + 1);
    twc_bin2hex(client_id,
                short_id_length / 2,
                hex_address);

    return hex_address;
}

/**
 * Reverse the bytes of a 32-bit integer.
 */
uint32_t
twc_uint32_reverse_bytes(uint32_t num)
{
    uint32_t res = 0;

    res += num & 0xFF; num >>= 8; res <<= 8;
    res += num & 0xFF; num >>= 8; res <<= 8;
    res += num & 0xFF; num >>= 8; res <<= 8;
    res += num & 0xFF;

    return res;
}
