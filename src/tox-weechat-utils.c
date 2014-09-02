#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

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
weechat_tox_bin2hex(const uint8_t *bin, size_t size)
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

