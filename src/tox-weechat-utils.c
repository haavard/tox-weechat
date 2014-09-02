#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"

#include "tox-weechat-utils.h"

uint8_t *
tox_weechat_hex2bin(const char *hex)
{
    size_t length = strlen(hex) / 2;
    uint8_t *bin = malloc(length);
    const char *position = hex;

    for (size_t i = 0; i < length; ++i)
    {
        sscanf(position, "%2hhx", &bin[i]);
        position += 2;
    }

    return bin;
}

char *
tox_weechat_bin2hex(const uint8_t *bin, size_t size)
{
    char *hex = malloc(size * 2 + 1);
    char *position = hex;

    for (size_t i = 0; i < size; ++i)
    {
        sprintf(position, "%02X", bin[i]);
        position += 2;
    }
    *position = 0;

    return hex;
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
tox_weechat_get_name_nt(int32_t friend_number)
{
    size_t length = tox_get_name_size(tox, friend_number);
    uint8_t name[length];
    tox_get_name(tox, friend_number, name);

    // if no name, return client ID instead
    if (weechat_utf8_strlen((char *)name) == 0)
    {
        uint8_t client_id[TOX_CLIENT_ID_SIZE];
        tox_get_client_id(tox, friend_number, client_id);
        return tox_weechat_bin2hex(client_id, TOX_CLIENT_ID_SIZE);
    }

    return tox_weechat_null_terminate(name, length);
}

char *
tox_weechat_get_status_message_nt(int32_t friend_number)
{
    size_t length = tox_get_status_message_size(tox, friend_number);
    uint8_t message[length];
    tox_get_status_message(tox, friend_number, message, length);

    return tox_weechat_null_terminate(message, length);
}

char *
tox_weechat_get_self_name_nt()
{
    size_t length = tox_get_self_name_size(tox);
    uint8_t name[length];
    tox_get_self_name(tox, name);

    return tox_weechat_null_terminate(name, length);
}
