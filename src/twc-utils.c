/*
 * Copyright (c) 2018 Håvard Pettersson <mail@haavard.me>
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

#include <stdio.h>
#include <string.h>

#include <tox/tox.h>
#include <weechat/weechat-plugin.h>

#include "twc-config.h"
#include "twc.h"

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
    TOX_ERR_FRIEND_QUERY err;
    size_t length = tox_friend_get_name_size(tox, friend_number, &err);

    if ((err != TOX_ERR_FRIEND_QUERY_OK) || (length == 0))
        return twc_get_friend_id_short(tox, friend_number);

    uint8_t name[length];

    tox_friend_get_name(tox, friend_number, name, &err);
    return twc_null_terminate(name, length);
}

/**
 * Return the null-terminated status message of a Tox friend. Must be freed.
 */
char *
twc_get_status_message_nt(Tox *tox, int32_t friend_number)
{
    TOX_ERR_FRIEND_QUERY err;
    size_t length =
        tox_friend_get_status_message_size(tox, friend_number, &err);

    if ((err != TOX_ERR_FRIEND_QUERY_OK) || (length == SIZE_MAX))
    {
        char *msg = malloc(1);
        *msg = 0;
        return msg;
    }

    uint8_t message[length];
    tox_friend_get_status_message(tox, friend_number, message, &err);

    return twc_null_terminate(message, length);
}

/**
 * Return the name of a group chat peer as a null terminated string. Must be
 * freed.
 */
char *
twc_get_peer_name_nt(Tox *tox, int32_t group_number, int32_t peer_number)
{
    uint8_t name[TOX_MAX_NAME_LENGTH + 1] = {0};
    TOX_ERR_CONFERENCE_PEER_QUERY err = TOX_ERR_CONFERENCE_PEER_QUERY_OK;

    int length =
        tox_conference_peer_get_name_size(tox, group_number, peer_number, &err);
    if ((err == TOX_ERR_CONFERENCE_PEER_QUERY_OK) &&
        (length <= TOX_MAX_NAME_LENGTH))
    {
        tox_conference_peer_get_name(tox, group_number, peer_number, name,
                                     &err);
        if (err == TOX_ERR_CONFERENCE_PEER_QUERY_OK)
            return twc_null_terminate(name, length);
        else
            return "<unknown>";
    }
    else
        return "<unknown>";
}

/**
 * Return the users own name, null-terminated. Must be freed.
 */
char *
twc_get_self_name_nt(Tox *tox)
{
    size_t length = tox_self_get_name_size(tox);
    uint8_t name[length];
    tox_self_get_name(tox, name);

    return twc_null_terminate(name, length);
}

/**
 * Return a friend's Tox ID in short form. Return value must be freed.
 */
char *
twc_get_friend_id_short(Tox *tox, int32_t friend_number)
{
    uint8_t client_id[TOX_PUBLIC_KEY_SIZE];
    TOX_ERR_FRIEND_GET_PUBLIC_KEY err;
    size_t short_id_length = weechat_config_integer(twc_config_short_id_size);
    char *hex_address = malloc(short_id_length + 1);

    tox_friend_get_public_key(tox, friend_number, client_id, &err);

    /* return a zero public key on failure */
    if (err != TOX_ERR_FRIEND_GET_PUBLIC_KEY_OK)
        memset(client_id, 0, TOX_PUBLIC_KEY_SIZE);

    twc_bin2hex(client_id, short_id_length / 2, hex_address);

    return hex_address;
}

/**
 * Return a group peer's Tox ID in short form. Return value must be freed.
 */
char *
twc_get_peer_id_short(Tox *tox, uint32_t conference_number,
                      uint32_t peer_number)
{
    uint8_t peer_id[TOX_PUBLIC_KEY_SIZE];
    TOX_ERR_CONFERENCE_PEER_QUERY err;
    size_t short_id_length = weechat_config_integer(twc_config_short_id_size);
    short_id_length =
        short_id_length > 4 ? short_id_length : 4; /* Let's use a sane value */
    char *hex_address = malloc(short_id_length + 1);

    tox_conference_peer_get_public_key(tox, conference_number, peer_number,
                                       peer_id, &err);
    if (err != TOX_ERR_CONFERENCE_PEER_QUERY_OK)
        memset(peer_id, 0, TOX_PUBLIC_KEY_SIZE);

    twc_bin2hex(peer_id, short_id_length / 2, hex_address);
    return hex_address;
}

/**
 * Prefix a nickname with Tox ID in short form. Return value must be freed.
 */
char *
twc_get_peer_name_prefixed(const char *id, const char *name)
{
    if (!name)
        name = "Tox User";
    size_t full_length = strlen(id) + strlen(name) + 4;
    char *full_name = malloc(full_length);
    snprintf(full_name, full_length, "[%s] %s", id, name);
    return full_name;
}

/**
 * Return amount of times a certain nickname repeats in a t_weelist list.
 */
size_t
twc_get_peer_name_count(struct t_weelist *list, const char *name)
{
    size_t count = 0;
    struct t_weelist_item *item = weechat_list_get(list, 0);
    for (item = weechat_list_get(list, 0); item; item = weechat_list_next(item))
    {
        if (!strcmp(weechat_list_string(item), name))
            count++;
    }
    return count;
}

/**
 * Return t_weelist* to a list of strings which start with a certain string,
 * NULL otherwise if there's no such strings. Must be properly freed after
 * usage.
 */
struct t_weelist *
twc_starts_with(struct t_weelist *list, const char *search,
                struct t_weelist *result)
{
    size_t length = strlen(search);
    weechat_list_remove_all(result);
    if (!search || !length)
        return result;
    struct t_weelist_item *item = weechat_list_get(list, 0);
    while (item)
    {
        const char *string = weechat_list_string(item);
        if (strlen(string) >= length && !strncmp(search, string, length))
            weechat_list_add(result, string, WEECHAT_LIST_POS_SORT, NULL);
        item = weechat_list_next(item);
    }
    return result;
}

/**
 * Return next completion string regarding of a previous one.
 */

const char *
twc_get_next_completion(struct t_weelist *completion_list,
                        const char *prev_comp)
{
    if (!weechat_list_size(completion_list))
        return NULL;
    const char *comp =
        weechat_list_string(weechat_list_get(completion_list, 0));
    if (prev_comp)
    {
        struct t_weelist_item *item =
            weechat_list_search(completion_list, prev_comp);
        if (item && (item = weechat_list_next(item)))
            comp = weechat_list_string(item);
    }
    return comp;
}

/**
 * Checks if an ID is ignored.
 */
bool
twc_is_id_ignored(struct t_twc_profile *profile, const char *short_id)
{
    struct t_weelist_item *ignore_item;
    for (ignore_item = weechat_list_get(profile->ignores, 0); ignore_item;
         ignore_item = weechat_list_next(ignore_item))
    {
        if (!weechat_strncasecmp(short_id, weechat_list_string(ignore_item),
                                 strlen(weechat_list_string(ignore_item))))
            return true;
    }
    return false;
}

/**
 * reverse the bytes of a 32-bit integer.
 */
uint32_t
twc_uint32_reverse_bytes(uint32_t num)
{
    uint32_t res = 0;

    res += num & 0xFF;
    num >>= 8;
    res <<= 8;
    res += num & 0xFF;
    num >>= 8;
    res <<= 8;
    res += num & 0xFF;
    num >>= 8;
    res <<= 8;
    res += num & 0xFF;

    return res;
}

/**
 * Fit correct unicode string into max chars. Return number of bytes
 */
int
twc_fit_utf8(const char *str, int max)
{
    return weechat_utf8_real_pos(str, weechat_utf8_strnlen(str, max));
}

/**
 * Enable or disable logging for a WeeChat buffer.
 */
int
twc_set_buffer_logging(struct t_gui_buffer *buffer, bool logging)
{
    if (!buffer)
        return WEECHAT_RC_ERROR;

    char const *signal;
    if (logging)
    {
        weechat_buffer_set(buffer, "localvar_del_no_log", "");
        signal = "logger_start";
    }
    else
    {
        weechat_buffer_set(buffer, "localvar_set_no_log", "1");
        signal = "logger_stop";
    }

    return weechat_hook_signal_send(signal, WEECHAT_HOOK_SIGNAL_POINTER,
                                    buffer);
}

/**
 * These following twc_tox_err_file_* functions convert enum TOX_ERR_FILE_*
 * error codes to meaningful messages of type char *.
 */

char *
twc_tox_err_file_control(enum TOX_ERR_FILE_CONTROL error)
{
    char *messages[] = {
        "success",
        "the friend number passed did not designate a valid friend",
        "this client is currently not connected to the friend",
        "no file transfer with the given file number was found for the given "
        "friend",
        "a RESUME control was sent, but the file transfer is running normally",
        "A RESUME control was sent, but the file transfer was paused by the "
        "other party",
        "a PAUSE control was sent, but the file transfer was already paused",
        "packet queue is full"};
    return messages[error];
}

char *
twc_tox_err_file_get(enum TOX_ERR_FILE_GET error)
{
    char *messages[] = {
        "success",
        "one of the arguments to the function was NULL when it was not "
        "expected",
        "the friend number passed did not designate a valid friend",
        "no file transfer with the given number was found for the given "
        "friend"};
    return messages[error];
}

char *
twc_tox_err_file_seek(enum TOX_ERR_FILE_SEEK error)
{
    char *messages[] = {
        "success",
        "the friend number passed did not designate a valid friend",
        "the client is currently not connected to the friend",
        "no file transfer with the given file number was found for the given "
        "friend",
        "file was not in a state where it could be seeked",
        "seek position was invalid",
        "packet queue is full"};
    return messages[error];
}

char *
twc_tox_err_file_send(enum TOX_ERR_FILE_SEND error)
{
    char *messages[] = {
        "success",
        "one of the arguments of the function was NULL when it was not "
        "expected",
        "the friend number passed did not designate a valid friend",
        "this client is currently not connected to the friend",
        "filename lenth exceeded TOX_MAX_FILENAME_LENGTH bytes",
        "too many ongoing transfers"};
    return messages[error];
}

char *
twc_tox_err_file_send_chunk(enum TOX_ERR_FILE_SEND_CHUNK error)
{
    char *messages[] = {
        "success",
        "the length parameter was non-zero, but data was NULL",
        "the friend number passed did not designate a valid friend",
        "this client is currently not connected to the friend",
        "no file transfer with the given file number was found for the given "
        "friend",
        "not called from the request chunk callback",
        "attempted to send more or less data than requested",
        "packet queue is full",
        "position parameter was wrong"};
    return messages[error];
}
