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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wordexp.h>

#include <tox/tox.h>
#include <weechat/weechat-plugin.h>

#include "twc-bootstrap.h"
#include "twc-chat.h"
#include "twc-config.h"
#include "twc-friend-request.h"
#include "twc-group-invite.h"
#include "twc-list.h"
#include "twc-profile.h"
#include "twc-tfer.h"
#include "twc-utils.h"
#include "twc.h"

#include "twc-commands.h"

enum TWC_FRIEND_MATCH
{
    TWC_FRIEND_MATCH_AMBIGUOUS = -1,
    TWC_FRIEND_MATCH_NOMATCH = -2,
};

/**
 * Make sure a command is executed on a Tox profile buffer. If not, warn user
 * and abort.
 */
#define TWC_CHECK_PROFILE(profile)                                             \
    if (!profile)                                                              \
    {                                                                          \
        weechat_printf(NULL,                                                   \
                       "%s%s: command \"%s\" must be executed on a Tox "       \
                       "buffer",                                               \
                       weechat_prefix("error"), weechat_plugin->name,          \
                       argv[0]);                                               \
        return WEECHAT_RC_OK;                                                  \
    }

/**
 * Make sure a command is executed in a chat buffer. If not, warn user and
 * abort.
 */
#define TWC_CHECK_CHAT(chat)                                                   \
    if (!chat)                                                                 \
    {                                                                          \
        weechat_printf(NULL,                                                   \
                       "%s%s: command \"%s\" must be executed in a chat "      \
                       "buffer",                                               \
                       weechat_prefix("error"), weechat_plugin->name,          \
                       argv[0]);                                               \
        return WEECHAT_RC_OK;                                                  \
    }

/**
 * Make sure a command is executed in a group chat buffer. If not, warn user
 * and abort.
 */
#define TWC_CHECK_GROUP_CHAT(chat)                                             \
    if (!chat || chat->group_number < 0)                                       \
    {                                                                          \
        weechat_printf(NULL,                                                   \
                       "%s%s: command \"%s\" must be executed in a group "     \
                       "chat buffer ",                                         \
                       weechat_prefix("error"), weechat_plugin->name,          \
                       argv[0]);                                               \
        return WEECHAT_RC_OK;                                                  \
    }

/**
 * Make sure a profile with the given name exists. If not, warn user and abort.
 */
#define TWC_CHECK_PROFILE_EXISTS(profile)                                      \
    if (!profile)                                                              \
    {                                                                          \
        weechat_printf(NULL, "%s%s: profile \"%s\" does not exist.",           \
                       weechat_prefix("error"), weechat_plugin->name, name);   \
        return WEECHAT_RC_OK;                                                  \
    }

/**
 * Make sure a profile is loaded.
 */
#define TWC_CHECK_PROFILE_LOADED(profile)                                      \
    if (!(profile->tox))                                                       \
    {                                                                          \
        weechat_printf(profile->buffer,                                        \
                       "%sprofile must be loaded for command \"%s\"",          \
                       weechat_prefix("error"), argv[0]);                      \
        return WEECHAT_RC_OK;                                                  \
    }

/**
 * Make sure friend exists.
 */
#define TWC_CHECK_FRIEND_NUMBER(profile, number, string)                       \
    if (number == TWC_FRIEND_MATCH_NOMATCH)                                    \
    {                                                                          \
        weechat_printf(profile->buffer,                                        \
                       "%sno friend number, name or Tox ID found matching "    \
                       "\"%s\"",                                               \
                       weechat_prefix("error"), string);                       \
        return WEECHAT_RC_OK;                                                  \
    }                                                                          \
    if (number == TWC_FRIEND_MATCH_AMBIGUOUS)                                  \
    {                                                                          \
        weechat_printf(profile->buffer,                                        \
                       "%smultiple friends with name  \"%s\" found; please "   \
                       "use Tox ID instead",                                   \
                       weechat_prefix("error"), string);                       \
        return WEECHAT_RC_OK;                                                  \
    }

/**
 * Make sure a file exists.
 */
#define TWC_CHECK_FILE_EXISTS(filename)                                        \
    if (access(filename, F_OK) == -1)                                          \
    {                                                                          \
        weechat_printf(NULL, "%sFile \"%s\" does not exist",                   \
                       weechat_prefix("error"), filename);                     \
        return WEECHAT_RC_ERROR;                                               \
    }

#define TWC_RETURN_WITH_FILE_ERROR(filename, type)                             \
    weechat_printf(NULL,                                                       \
                   "%s\"%s\" must be a regular file or pipe, "                 \
                   "not a %s",                                                 \
                   weechat_prefix("error"), filename, type);                   \
    return WEECHAT_RC_ERROR;

/**
 * Get number of friend matching string. Tries to match number, name and
 * Tox ID.
 */
enum TWC_FRIEND_MATCH
twc_match_friend(struct t_twc_profile *profile, const char *search_string)
{
    uint32_t friend_count = tox_self_get_friend_list_size(profile->tox);
    uint32_t *friend_numbers = malloc(sizeof(uint32_t) * friend_count);
    tox_self_get_friend_list(profile->tox, friend_numbers);

    int32_t match = TWC_FRIEND_MATCH_NOMATCH;

    char *endptr;
    uint32_t friend_number = (uint32_t)strtoul(search_string, &endptr, 10);
    if (endptr == search_string + strlen(search_string) &&
        tox_friend_exists(profile->tox, friend_number))
        return friend_number;

    size_t search_size = strlen(search_string);
    for (uint32_t i = 0; i < friend_count; ++i)
    {
        if (search_size == TOX_PUBLIC_KEY_SIZE * 2)
        {
            uint8_t tox_id[TOX_PUBLIC_KEY_SIZE];
            char hex_id[TOX_PUBLIC_KEY_SIZE * 2 + 1];

            if (tox_friend_get_public_key(profile->tox, friend_numbers[i],
                                          tox_id, NULL))
            {
                twc_bin2hex(tox_id, TOX_PUBLIC_KEY_SIZE, hex_id);

                if (weechat_strcasecmp(hex_id, search_string) == 0)
                    return friend_numbers[i];
            }
        }

        char *name = twc_get_name_nt(profile->tox, friend_numbers[i]);
        if (weechat_strcasecmp(name, search_string) == 0)
        {
            if (match == TWC_FRIEND_MATCH_NOMATCH)
                match = friend_numbers[i];
            else
                return TWC_FRIEND_MATCH_AMBIGUOUS;
        }
    }

    return match;
}

/**
 * Command /bootstrap callback.
 */
int
twc_cmd_bootstrap(const void *pointer, void *data, struct t_gui_buffer *buffer,
                  int argc, char **argv, char **argv_eol)
{
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    /* /bootstrap connect <address> <port> <key> */
    if (argc == 5 && weechat_strcasecmp(argv[1], "connect") == 0)
    {
        char *address = argv[2];
        uint16_t port = atoi(argv[3]);
        char *public_key = argv[4];

        if (!twc_bootstrap_tox(profile->tox, address, port, public_key))
        {
            weechat_printf(profile->buffer,
                           "%sBootstrap could not open address \"%s\"",
                           weechat_prefix("error"), address);
        }

        return WEECHAT_RC_OK;
    }

    return WEECHAT_RC_ERROR;
}

/**
 * Command /friend callback.
 */
int
twc_cmd_friend(const void *pointer, void *data, struct t_gui_buffer *buffer,
               int argc, char **argv, char **argv_eol)
{
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    /* /friend or /friend list */
    if (argc == 1 || (argc == 2 && weechat_strcasecmp(argv[1], "list") == 0))
    {
        size_t friend_count = tox_self_get_friend_list_size(profile->tox);
        uint32_t friend_numbers[friend_count];
        tox_self_get_friend_list(profile->tox, friend_numbers);

        if (friend_count == 0)
        {
            weechat_printf(profile->buffer, "%sYou have no friends :(",
                           weechat_prefix("network"));
            return WEECHAT_RC_OK;
        }

        weechat_printf(profile->buffer, "%s[#] Name [Tox ID (short)] Status",
                       weechat_prefix("network"));

        for (size_t i = 0; i < friend_count; ++i)
        {
            uint32_t friend_number = friend_numbers[i];
            char *name = twc_get_name_nt(profile->tox, friend_number);
            char *hex_address =
                twc_get_friend_id_short(profile->tox, friend_number);

            char *status =
                twc_get_status_message_nt(profile->tox, friend_number);
            char *online_color =
                (tox_friend_get_connection_status(profile->tox, friend_number,
                                                  NULL) != TOX_CONNECTION_NONE)
                    ? "chat_nick"
                    : "chat_nick_offline";
            weechat_printf(profile->buffer, "%s[%" PRIu32 "] %s%s [%s]%s %s",
                           weechat_prefix("network"), friend_number,
                           weechat_color(online_color), name, hex_address,
                           weechat_color("reset"), status);

            free(name);
            free(status);
            free(hex_address);
        }

        return WEECHAT_RC_OK;
    }

    /* /friend add [-force] <Tox ID> [<message>] */
    else if (argc >= 3 && weechat_strcasecmp(argv[1], "add") == 0)
    {
        bool force;
        const char *hex_id;
        const char *message;

        force = weechat_strcasecmp(argv[2], "-force") == 0;
        if (force)
        {
            hex_id = argv[3];
            message = argc >= 5 ? argv_eol[4] : NULL;
        }
        else
        {
            hex_id = argv[2];
            message = argc >= 4 ? argv_eol[3] : NULL;
        }

        if (!message)
            message = weechat_config_string(twc_config_friend_request_message);

        if (strlen(hex_id) != TOX_ADDRESS_SIZE * 2)
        {
            weechat_printf(profile->buffer,
                           "%sTox ID length invalid. Please try again.",
                           weechat_prefix("error"));

            return WEECHAT_RC_OK;
        }

        uint8_t address[TOX_ADDRESS_SIZE];
        twc_hex2bin(hex_id, TOX_ADDRESS_SIZE, address);

        if (force)
        {
            bool fail = false;
            char *hex_key = strndup(hex_id, TOX_PUBLIC_KEY_SIZE * 2);
            int32_t friend_number = twc_match_friend(profile, hex_key);
            free(hex_key);

            if (friend_number == TWC_FRIEND_MATCH_AMBIGUOUS)
                fail = true;
            else if (friend_number != TWC_FRIEND_MATCH_NOMATCH)
                fail = !tox_friend_delete(profile->tox, friend_number, NULL);

            if (fail)
            {
                weechat_printf(profile->buffer,
                               "%scould not remove friend; please remove "
                               "manually before resending friend request",
                               weechat_prefix("error"));
                return WEECHAT_RC_OK;
            }
        }

        TOX_ERR_FRIEND_ADD err;
        (void)tox_friend_add(profile->tox, (uint8_t *)address,
                             (uint8_t *)message, strlen(message), &err);

        switch (err)
        {
            case TOX_ERR_FRIEND_ADD_OK:
                weechat_printf(profile->buffer, "%sFriend request sent!",
                               weechat_prefix("network"));
                break;
            case TOX_ERR_FRIEND_ADD_TOO_LONG:
                weechat_printf(profile->buffer,
                               "%sFriend request message too long! Try again.",
                               weechat_prefix("error"));
                break;
            case TOX_ERR_FRIEND_ADD_ALREADY_SENT:
            case TOX_ERR_FRIEND_ADD_SET_NEW_NOSPAM:
                weechat_printf(profile->buffer,
                               "%sYou have already sent a friend request to "
                               "that address (use -force to circumvent)",
                               weechat_prefix("error"));
                break;
            case TOX_ERR_FRIEND_ADD_OWN_KEY:
                weechat_printf(profile->buffer,
                               "%sYou can't add yourself as a friend.",
                               weechat_prefix("error"));
                break;
            case TOX_ERR_FRIEND_ADD_BAD_CHECKSUM:
                weechat_printf(profile->buffer,
                               "%sInvalid friend address - try again.",
                               weechat_prefix("error"));
                break;
            case TOX_ERR_FRIEND_ADD_MALLOC:
                weechat_printf(profile->buffer,
                               "%sCould not add friend (out of memory).",
                               weechat_prefix("error"));
                break;
            case TOX_ERR_FRIEND_ADD_NULL:
            case TOX_ERR_FRIEND_ADD_NO_MESSAGE: /* this should not happen as we
                                                   validate the message */
            default:
                weechat_printf(profile->buffer,
                               "%sCould not add friend (unknown error %d).",
                               weechat_prefix("error"), err);
                break;
        }

        return WEECHAT_RC_OK;
    }

    /* /friend remove */
    else if (argc >= 3 && (weechat_strcasecmp(argv[1], "remove") == 0))
    {
        int32_t friend_number = twc_match_friend(profile, argv[2]);
        TWC_CHECK_FRIEND_NUMBER(profile, friend_number, argv[2]);

        char *name = twc_get_name_nt(profile->tox, friend_number);
        if (tox_friend_delete(profile->tox, friend_number, NULL))
        {
            weechat_printf(profile->buffer, "%sRemoved %s from friend list.",
                           weechat_prefix("network"), name);
        }
        else
        {
            weechat_printf(profile->buffer, "%sCould not remove friend!",
                           weechat_prefix("error"));
        }

        free(name);

        return WEECHAT_RC_OK;
    }

    /* friend accept|decline <number>|all */
    else if (argc == 3 && (weechat_strcasecmp(argv[1], "accept") == 0 ||
                           weechat_strcasecmp(argv[1], "decline") == 0))
    {
        int accept = weechat_strcasecmp(argv[1], "accept") == 0;

        struct t_twc_friend_request *request;
        if (weechat_strcasecmp(argv[2], "all") == 0)
        {
            size_t index;
            size_t count = 0;
            struct t_twc_list_item *item;
            twc_list_foreach (profile->friend_requests, index, item)
            {
                if (accept)
                {
                    if (twc_friend_request_accept(item->friend_request))
                    {
                        ++count;
                    }
                    else
                    {
                        char hex_address[TOX_PUBLIC_KEY_SIZE * 2 + 1];
                        twc_bin2hex(item->friend_request->tox_id,
                                    TOX_PUBLIC_KEY_SIZE, hex_address);
                        weechat_printf(
                            profile->buffer,
                            "%sCould not accept friend request from %s",
                            weechat_prefix("error"), hex_address);
                    }
                }
                else
                {
                    twc_friend_request_remove(item->friend_request);
                    ++count;
                }
            }

            weechat_printf(profile->buffer, "%s%s %zu friend requests.",
                           weechat_prefix("network"),
                           accept ? "Accepted" : "Declined", count);

            return WEECHAT_RC_OK;
        }
        else
        {
            char *endptr;
            unsigned long num = strtoul(argv[2], &endptr, 10);
            if (endptr == argv[2] ||
                (request = twc_friend_request_with_index(profile, num)) == NULL)
            {
                weechat_printf(profile->buffer, "%sInvalid friend request ID.",
                               weechat_prefix("error"));
                return WEECHAT_RC_OK;
            }

            char hex_address[TOX_PUBLIC_KEY_SIZE * 2 + 1];
            twc_bin2hex(request->tox_id, TOX_PUBLIC_KEY_SIZE, hex_address);

            if (accept)
            {
                if (!twc_friend_request_accept(request))
                {
                    weechat_printf(profile->buffer,
                                   "%sCould not accept friend request from %s",
                                   weechat_prefix("error"), hex_address);
                }
                else
                {
                    weechat_printf(profile->buffer,
                                   "%sAccepted friend request from %s.",
                                   weechat_prefix("network"), hex_address);
                }
            }
            else
            {
                twc_friend_request_remove(request);
                weechat_printf(profile->buffer,
                               "%sDeclined friend request from %s.",
                               weechat_prefix("network"), hex_address);
            }

            twc_friend_request_free(request);

            return WEECHAT_RC_OK;
        }
    }

    /* /friend requests */
    else if (argc == 2 && weechat_strcasecmp(argv[1], "requests") == 0)
    {
        weechat_printf(profile->buffer,
                       "%sPending friend requests:", weechat_prefix("network"));

        size_t index;
        struct t_twc_list_item *item;
        twc_list_foreach (profile->friend_requests, index, item)
        {
            char hex_address[TOX_PUBLIC_KEY_SIZE * 2 + 1];
            twc_bin2hex(item->friend_request->tox_id, TOX_PUBLIC_KEY_SIZE,
                        hex_address);

            weechat_printf(profile->buffer,
                           "%s[%zu] Address: %s\n"
                           "[%zu] Message: %s",
                           weechat_prefix("network"), index, hex_address, index,
                           item->friend_request->message);
        }

        return WEECHAT_RC_OK;
    }

    return WEECHAT_RC_ERROR;
}

/**
 * Command /group callback.
 */
int
twc_cmd_group(const void *pointer, void *data, struct t_gui_buffer *buffer,
              int argc, char **argv, char **argv_eol)
{
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TOX_ERR_CONFERENCE_NEW err = TOX_ERR_CONFERENCE_NEW_OK;
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    /* /group create */
    if (argc == 2 && weechat_strcasecmp(argv[1], "create") == 0)
    {
        int rc = tox_conference_new(profile->tox, &err);
        if (err == TOX_ERR_CONFERENCE_NEW_OK)
            twc_chat_search_group(profile, rc, true);
        else
            weechat_printf(profile->buffer,
                           "%sCould not create group chat with error %d",
                           weechat_prefix("error"), err);

        return WEECHAT_RC_OK;
    }

    /* /group join|decline <number> */
    else if (argc == 3 && (weechat_strcasecmp(argv[1], "join") == 0 ||
                           weechat_strcasecmp(argv[1], "decline") == 0))
    {
        bool join = weechat_strcasecmp(argv[1], "join") == 0;

        struct t_twc_group_chat_invite *invite;

        char *endptr;
        unsigned long num = strtoul(argv[2], &endptr, 10);
        if (endptr == argv[2] ||
            (invite = twc_group_chat_invite_with_index(profile, num)) == NULL)
        {
            weechat_printf(profile->buffer, "%sInvalid group chat invite ID.",
                           weechat_prefix("error"));
            return WEECHAT_RC_OK;
        }

        if (join)
        {
            int group_number = twc_group_chat_invite_join(invite);

            /* create a buffer for the new group chat */
            if (group_number >= 0)
                twc_chat_search_group(profile, group_number, true);
            else
                weechat_printf(profile->buffer,
                               "%sCould not join group chat (unknown error)",
                               weechat_prefix("error"));
        }
        else
        {
            twc_group_chat_invite_remove(invite);
        }

        return WEECHAT_RC_OK;
    }

    /* /group invites */
    else if (argc == 2 && weechat_strcasecmp(argv[1], "invites") == 0)
    {
        weechat_printf(profile->buffer, "%sPending group chat invites:",
                       weechat_prefix("network"));

        size_t index;
        struct t_twc_list_item *item;
        twc_list_foreach (profile->group_chat_invites, index, item)
        {
            char *friend_name = twc_get_name_nt(
                profile->tox, item->group_chat_invite->friend_number);
            weechat_printf(profile->buffer, "%s[%zu] From: %s",
                           weechat_prefix("network"), index, friend_name);
            free(friend_name);
        }

        return WEECHAT_RC_OK;
    }

    return WEECHAT_RC_ERROR;
}

/**
 * Command /invite callback.
 */
int
twc_cmd_invite(const void *pointer, void *data, struct t_gui_buffer *buffer,
               int argc, char **argv, char **argv_eol)
{
    if (argc == 1)
        return WEECHAT_RC_ERROR;

    TOX_ERR_CONFERENCE_INVITE err = TOX_ERR_CONFERENCE_INVITE_OK;

    struct t_twc_chat *chat = twc_chat_search_buffer(buffer);
    TWC_CHECK_PROFILE_LOADED(chat->profile);
    TWC_CHECK_GROUP_CHAT(chat);

    int32_t friend_number = twc_match_friend(chat->profile, argv_eol[1]);
    TWC_CHECK_FRIEND_NUMBER(chat->profile, friend_number, argv_eol[1]);

    tox_conference_invite(chat->profile->tox, friend_number, chat->group_number,
                          &err);

    if (err == TOX_ERR_CONFERENCE_INVITE_OK)
    {
        char *friend_name = twc_get_name_nt(chat->profile->tox, friend_number);
        weechat_printf(chat->buffer, "%sInvited %s to the group chat.",
                       weechat_prefix("network"), friend_name);
        free(friend_name);
    }
    else
    {
        weechat_printf(chat->buffer,
                       "%sFailed to send group chat invite with error %d",
                       weechat_prefix("error"), err);
    }

    return WEECHAT_RC_OK;
}

/**
 * Command /me callback.
 */
int
twc_cmd_me(const void *pointer, void *data, struct t_gui_buffer *buffer,
           int argc, char **argv, char **argv_eol)
{
    if (argc == 1)
        return WEECHAT_RC_ERROR;

    struct t_twc_chat *chat = twc_chat_search_buffer(buffer);
    TWC_CHECK_CHAT(chat);
    TWC_CHECK_PROFILE_LOADED(chat->profile);

    twc_chat_send_message(chat, argv_eol[1], TOX_MESSAGE_TYPE_ACTION);

    return WEECHAT_RC_OK;
}
/**
 * Command /msg callback.
 */
int
twc_cmd_msg(const void *pointer, void *data, struct t_gui_buffer *buffer,
            int argc, char **argv, char **argv_eol)
{
    if (argc == 1)
        return WEECHAT_RC_ERROR;

    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    /* do a shell split in case a friend has spaces in his name and we need
     * quotes */
    int shell_argc;
    char **shell_argv = weechat_string_split_shell(argv_eol[0], &shell_argc);

    char recipient[TOX_MAX_NAME_LENGTH + 1];
    snprintf(recipient, TOX_MAX_NAME_LENGTH, "%s", shell_argv[1]);
    weechat_string_free_split(shell_argv);

    char *message = NULL;
    if (shell_argc >= 3)
    {
        /* extract message, add two if quotes are used */
        message = argv_eol[1] + strlen(recipient);
        if (*argv[1] == '"' || *argv[1] == '\'')
            message += 2;
    }

    int32_t friend_number = twc_match_friend(profile, recipient);
    TWC_CHECK_FRIEND_NUMBER(profile, friend_number, recipient);

    /* create chat buffer if it does not exist */
    struct t_twc_chat *chat =
        twc_chat_search_friend(profile, friend_number, true);

    /* send a message if provided */
    if (message)
        twc_chat_send_message(chat, weechat_string_strip(message, 1, 1, " "),
                              TOX_MESSAGE_TYPE_NORMAL);

    return WEECHAT_RC_OK;
}

/**
 * Command /myid callback.
 */
int
twc_cmd_myid(const void *pointer, void *data, struct t_gui_buffer *buffer,
             int argc, char **argv, char **argv_eol)
{
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    uint8_t address[TOX_ADDRESS_SIZE];
    tox_self_get_address(profile->tox, address);

    char address_str[TOX_ADDRESS_SIZE * 2 + 1];
    twc_bin2hex(address, TOX_ADDRESS_SIZE, address_str);

    weechat_printf(profile->buffer, "%sYour Tox address: %s",
                   weechat_prefix("network"), address_str);

    return WEECHAT_RC_OK;
}

/**
 * Command /name callback.
 */
int
twc_cmd_name(const void *pointer, void *data, struct t_gui_buffer *buffer,
             int argc, char **argv, char **argv_eol)
{
    if (argc == 1)
        return WEECHAT_RC_ERROR;

    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    const char *name = argv_eol[1];

    TOX_ERR_SET_INFO err;
    tox_self_set_name(profile->tox, (uint8_t *)name, strlen(name), &err);
    if (err != TOX_ERR_SET_INFO_OK)
    {
        char *err_msg;
        switch (err)
        {
            case TOX_ERR_SET_INFO_NULL:
                err_msg = "no name given";
                break;
            case TOX_ERR_SET_INFO_TOO_LONG:
                err_msg = "name too long";
                break;
            default:
                err_msg = "unknown error";
                break;
        }
        weechat_printf(profile->buffer, "%s%s%s", weechat_prefix("error"),
                       "Could not change name: ", err_msg);
        return WEECHAT_RC_OK;
    }

    weechat_bar_item_update("input_prompt");

    weechat_printf(profile->buffer, "%sYou are now known as %s",
                   weechat_prefix("network"), name);

    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach (profile->chats, index, item)
    {
        weechat_printf(item->chat->buffer, "%sYou are now known as %s",
                       weechat_prefix("network"), name);
    }

    return WEECHAT_RC_OK;
}

/**
 * Command /names callback.
 */
int
twc_cmd_names(const void *pointer, void *data, struct t_gui_buffer *buffer,
              int argc, char **argv, char **argv_eol)
{
    struct t_twc_chat *chat = twc_chat_search_buffer(buffer);
    TWC_CHECK_CHAT(chat);
    TWC_CHECK_PROFILE_LOADED(chat->profile);

    if (chat->group_number < 0)
    {
        weechat_printf(NULL,
                       "%s%s: command \"%s\" must be executed in a group chat "
                       "buffer",
                       weechat_prefix("error"), weechat_plugin->name, argv[0]);
        return WEECHAT_RC_OK;
    }

    size_t const num_names = weechat_list_size(chat->nicks);

    if (num_names == 0)
        return WEECHAT_RC_ERROR;

    char const *names[num_names];
    char const *colors[num_names];
    size_t total_names_length = 0;

    /* iterate over all names, retrieving length and string representation */
    struct t_weelist_item *name = weechat_list_get(chat->nicks, 0);
    size_t index = 0;
    while (name != NULL)
    {
        char const *const name_string = weechat_list_string(name);
        char const *const color = weechat_info_get("nick_color", name_string);

        names[index] = name_string;
        colors[index] = color;

        total_names_length += strlen(name_string) + strlen(color);

        ++index;
        name = weechat_list_next(name);
    }

    /* allocate space for all names, plus spaces and colours, plus \0 */
    char *const names_str = malloc(total_names_length + num_names);
    if (!names_str)
        return WEECHAT_RC_ERROR;

    /* copy all names into name_str buffer, with nick coloring */
    size_t loc = 0;
    for (size_t index = 0; index < num_names; ++index)
    {
        if (loc > 0)
            names_str[loc++] = ' ';

        char const *const name = names[index];
        char const *const color = colors[index];

        strcpy(names_str + loc, color);
        loc += strlen(color);

        strcpy(names_str + loc, name);
        loc += strlen(name);
    }

    weechat_printf(chat->buffer, "%s%zu names: %s", weechat_prefix("network"),
                   num_names, names_str);

    free(names_str);
    return WEECHAT_RC_OK;
}

/**
 * Command /nospam callback.
 */
int
twc_cmd_nospam(const void *pointer, void *data, struct t_gui_buffer *buffer,
               int argc, char **argv, char **argv_eol)
{
    if (argc > 2)
        return WEECHAT_RC_ERROR;

    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    uint32_t new_nospam;
    if (argc == 2)
    {
        char *endptr;
        unsigned long value = strtoul(argv[1], &endptr, 16);
        if (endptr == argv[1] || value > UINT32_MAX)
        {
            weechat_printf(profile->buffer,
                           "%snospam must be a hexadecimal value between "
                           "0x00000000 and 0xFFFFFFFF",
                           weechat_prefix("error"));

            return WEECHAT_RC_OK;
        }

        /* reverse the value bytes so it's displayed as entered in the Tox ID */
        new_nospam = twc_uint32_reverse_bytes(value);
    }
    else
    {
        new_nospam = random();
    }

    uint32_t old_nospam = tox_self_get_nospam(profile->tox);
    tox_self_set_nospam(profile->tox, new_nospam);

    weechat_printf(profile->buffer,
                   "%snew nospam has been set; this changes your Tox ID! To "
                   "revert, run \"/nospam %x\"",
                   weechat_prefix("network"),
                   twc_uint32_reverse_bytes(old_nospam));

    return WEECHAT_RC_OK;
}

/**
 * Command /part callback.
 */
int
twc_cmd_part(const void *pointer, void *data, struct t_gui_buffer *buffer,
             int argc, char **argv, char **argv_eol)
{
    struct t_twc_chat *chat = twc_chat_search_buffer(buffer);
    TOX_ERR_CONFERENCE_DELETE err = TOX_ERR_CONFERENCE_DELETE_OK;
    TWC_CHECK_PROFILE_LOADED(chat->profile);
    TWC_CHECK_GROUP_CHAT(chat);

    tox_conference_delete(chat->profile->tox, chat->group_number, &err);
    if (err == TOX_ERR_CONFERENCE_DELETE_OK)
    {
        weechat_printf(chat->buffer, "%sYou have left the group chat",
                       weechat_prefix("network"));
    }
    else
    {
        weechat_printf(chat->buffer,
                       "%sFailed to leave group chat with error %d",
                       weechat_prefix("error"), err);
    }

    weechat_buffer_set_pointer(chat->buffer, "input_callback", NULL);
    weechat_buffer_set_pointer(chat->buffer, "close_callback", NULL);

    twc_list_remove_with_data(chat->profile->chats, chat);
    twc_chat_free(chat);

    return WEECHAT_RC_OK;
}

/**
 * Save Tox profile data when /save is executed.
 */
int
twc_cmd_save(const void *pointer, void *data, struct t_gui_buffer *buffer,
             const char *command)
{
    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach (twc_profiles, index, item)
    {
        if (!(item->profile->tox))
            continue;

        int rc = twc_profile_save_data_file(item->profile);
        if (rc == -1)
        {
            weechat_printf(NULL, "%s%s: failed to save data for profile %s",
                           weechat_prefix("error"), weechat_plugin->name,
                           item->profile->name);
        }
    }

    weechat_printf(NULL, "%s: profile data saved", weechat_plugin->name);

    return WEECHAT_RC_OK;
}

/**
 * Command /status callback.
 */
int
twc_cmd_status(const void *pointer, void *data, struct t_gui_buffer *buffer,
               int argc, char **argv, char **argv_eol)
{
    if (argc != 2)
        return WEECHAT_RC_ERROR;

    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    TOX_USER_STATUS status;
    if (weechat_strcasecmp(argv[1], "online") == 0)
        status = TOX_USER_STATUS_NONE;
    else if (weechat_strcasecmp(argv[1], "busy") == 0)
        status = TOX_USER_STATUS_BUSY;
    else if (weechat_strcasecmp(argv[1], "away") == 0)
        status = TOX_USER_STATUS_AWAY;
    else
        return WEECHAT_RC_ERROR;

    tox_self_set_status(profile->tox, status);
    weechat_bar_item_update("away");

    return WEECHAT_RC_OK;
}

/**
 * Command /statusmsg callback.
 */
int
twc_cmd_statusmsg(const void *pointer, void *data, struct t_gui_buffer *buffer,
                  int argc, char **argv, char **argv_eol)
{
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    char *message = argc > 1 ? argv_eol[1] : " ";

    TOX_ERR_SET_INFO err;
    tox_self_set_status_message(profile->tox, (uint8_t *)message,
                                strlen(message), &err);
    if (err != TOX_ERR_SET_INFO_OK)
    {
        char *err_msg;
        switch (err)
        {
            case TOX_ERR_SET_INFO_NULL:
                err_msg = "no status given";
                break;
            case TOX_ERR_SET_INFO_TOO_LONG:
                err_msg = "status too long";
                break;
            default:
                err_msg = "unknown error";
                break;
        }
        weechat_printf(profile->buffer, "%s%s%s", weechat_prefix("error"),
                       "Could not set status message: ", err_msg);
    }

    return WEECHAT_RC_OK;
}

/**
 * Command /topic callback.
 */
int
twc_cmd_topic(const void *pointer, void *data, struct t_gui_buffer *buffer,
              int argc, char **argv, char **argv_eol)
{
    if (argc == 1)
        return WEECHAT_RC_ERROR;

    TOX_ERR_CONFERENCE_TITLE err = TOX_ERR_CONFERENCE_TITLE_OK;

    struct t_twc_chat *chat = twc_chat_search_buffer(buffer);
    TWC_CHECK_GROUP_CHAT(chat);
    TWC_CHECK_PROFILE_LOADED(chat->profile);

    char *topic = argv_eol[1];

    tox_conference_set_title(chat->profile->tox, chat->group_number,
                             (uint8_t *)topic, strlen(topic), &err);
    if (err != TOX_ERR_CONFERENCE_TITLE_OK)
    {
        weechat_printf(chat->buffer, "%s%s", weechat_prefix("error"),
                       "Could not set topic.");
        return WEECHAT_RC_OK;
    }

    twc_chat_queue_refresh(chat);

    return WEECHAT_RC_OK;
}

/**
 * Command /tox callback.
 */
int
twc_cmd_tox(const void *pointer, void *data, struct t_gui_buffer *buffer,
            int argc, char **argv, char **argv_eol)
{
    /* /tox [list] */
    if (argc == 1 || (argc == 2 && weechat_strcasecmp(argv[1], "list") == 0))
    {
        weechat_printf(NULL, "%sAll Tox profiles:", weechat_prefix("network"));
        size_t index;
        struct t_twc_list_item *item;
        twc_list_foreach (twc_profiles, index, item)
        {
            weechat_printf(NULL, "%s%s", weechat_prefix("network"),
                           item->profile->name);
        }

        return WEECHAT_RC_OK;
    }

    /* /tox create */
    else if (argc == 3 && (weechat_strcasecmp(argv[1], "create") == 0))
    {
        char *name = argv[2];

        if (twc_profile_search_name(name))
        {
            weechat_printf(NULL, "%s%s: profile \"%s\" already exists!",
                           weechat_prefix("error"), weechat_plugin->name, name);
            return WEECHAT_RC_OK;
        }

        struct t_twc_profile *profile = twc_profile_new(name);
        weechat_printf(NULL, "%s%s: profile \"%s\" created!",
                       weechat_prefix("network"), weechat_plugin->name,
                       profile->name);

        return WEECHAT_RC_OK;
    }

    /* /tox delete */
    else if ((argc == 3 || argc == 4) &&
             (weechat_strcasecmp(argv[1], "delete") == 0))
    {
        char *name = argv[2];
        char *flag = argv[3];

        struct t_twc_profile *profile = twc_profile_search_name(name);
        TWC_CHECK_PROFILE_EXISTS(profile);

        if (argc == 4 && strcmp(flag, "-keepdata") == 0)
        {
            twc_profile_delete(profile, false);
        }
        else if (argc == 4 && strcmp(flag, "-yes") == 0)
        {
            twc_profile_delete(profile, true);
        }
        else
        {
            weechat_printf(NULL,
                           "%s%s: You must confirm deletion with either "
                           "\"-keepdata\" or \"-yes\" (see /help tox)",
                           weechat_prefix("error"), weechat_plugin->name);
            return WEECHAT_RC_OK;
        }

        weechat_printf(NULL, "%s%s: profile \"%s\" has been deleted.",
                       weechat_prefix("error"), weechat_plugin->name, name);

        return WEECHAT_RC_OK;
    }

    /* /tox load|unload|reload [<profile>...] */
    else if (argc >= 2 && (weechat_strcasecmp(argv[1], "load") == 0 ||
                           weechat_strcasecmp(argv[1], "unload") == 0 ||
                           weechat_strcasecmp(argv[1], "reload") == 0))
    {
        bool load = weechat_strcasecmp(argv[1], "load") == 0;
        bool unload = weechat_strcasecmp(argv[1], "unload") == 0;
        bool reload = weechat_strcasecmp(argv[1], "reload") == 0;

        if (argc == 2)
        {
            struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
            if (!profile)
                return WEECHAT_RC_ERROR;

            if (unload || reload)
                twc_profile_unload(profile);
            if (load || reload)
                twc_profile_load(profile);
        }
        else
        {
            for (int i = 2; i < argc; ++i)
            {
                char *name = argv[i];
                struct t_twc_profile *profile = twc_profile_search_name(name);
                TWC_CHECK_PROFILE_EXISTS(profile);

                if (unload || reload)
                    twc_profile_unload(profile);
                if (load || reload)
                    twc_profile_load(profile);
            }
        }

        return WEECHAT_RC_OK;
    }

    return WEECHAT_RC_ERROR;
}

/**
 * Command /send callback.
 */
int
twc_cmd_send(const void *pointer, void *data, struct t_gui_buffer *buffer,
             int argc, char **argv, char **argv_eol)
{
    if (argc == 1)
        return WEECHAT_RC_ERROR;
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    char recipient[TOX_MAX_NAME_LENGTH + 1] = {0};
    char filename[FILENAME_MAX + 1] = {0};
    size_t filename_arg_num;

    /* /send <file> */
    if (argc == 2)
    {
        if (profile->buffer == buffer || profile->tfer->buffer == buffer)
        {
            weechat_printf(profile->buffer, "%s%s", weechat_prefix("error"),
                           "you must specify a friend");
            return WEECHAT_RC_ERROR;
        }
        snprintf(recipient, TOX_MAX_NAME_LENGTH, "%s",
                 weechat_buffer_get_string(buffer, "name"));
        struct t_twc_chat *chat = twc_chat_search_buffer(buffer);
        if (chat->group_number != -1)
        {
            weechat_printf(profile->buffer, "%s%s", weechat_prefix("error"),
                           "the file transmission is "
                           "allowed only between friends");
            return WEECHAT_RC_ERROR;
        }

        char *name = twc_get_name_nt(profile->tox, chat->friend_number);
        sprintf(recipient, "%s", name);
        filename_arg_num = 1;
        free(name);
    }

    /* /send <number>|<name>|<Tox ID> <file> */
    if (argc >= 3)
    {
        /* do a shell split in case a friend has spaces in his name
         * and join the name */
        int shell_argc;
        char **shell_argv =
            weechat_string_split_shell(argv_eol[1], &shell_argc);
        for (int i = 0; i < shell_argc - 1; i++)
        {
            strcat(recipient, shell_argv[i]);
            if (i < shell_argc - 2)
                strcat(recipient, " ");
        }
        filename_arg_num = shell_argc;
        weechat_string_free_split(shell_argv);
    }
    wordexp_t expanded;
    wordexp(argv[filename_arg_num], &expanded, 0);
    snprintf(filename, FILENAME_MAX, "%s", expanded.we_wordv[0]);
    wordfree(&expanded);
    TWC_CHECK_FILE_EXISTS(filename);

    uint32_t friend_number = twc_match_friend(profile, recipient);
    TWC_CHECK_FRIEND_NUMBER(profile, (signed)friend_number, recipient);

    struct stat st;
    stat(filename, &st);
    switch (st.st_mode & S_IFMT)
    {
        case S_IFBLK:
            TWC_RETURN_WITH_FILE_ERROR(filename, "block device");
        case S_IFCHR:
            TWC_RETURN_WITH_FILE_ERROR(filename, "character device");
        case S_IFDIR:
            TWC_RETURN_WITH_FILE_ERROR(filename, "directory");
        case S_IFSOCK:
            TWC_RETURN_WITH_FILE_ERROR(filename, "socket");
        case S_IFREG:
            break;
        case S_IFLNK:
            break;
        default:
            weechat_printf(NULL, "%sunknown file type",
                           weechat_prefix("error"));
            return WEECHAT_RC_ERROR;
    }

    char *stripped_name =
        twc_tfer_file_name_strip(filename, FILENAME_MAX + 1 - strlen(filename));

    TOX_ERR_FILE_SEND error;
    uint32_t file_number =
        tox_file_send(profile->tox, friend_number, TOX_FILE_KIND_DATA,
                      S_ISFIFO(st.st_mode) ? UINT64_MAX : (size_t)st.st_size,
                      NULL, (uint8_t *)stripped_name, strlen(filename), &error);
    free(stripped_name);
    if (error != TOX_ERR_FILE_SEND_OK)
    {
        weechat_printf(profile->buffer, "%ssending \"%s\" has been failed: %s",
                       weechat_prefix("error"), filename,
                       twc_tox_err_file_send(error));
        return WEECHAT_RC_ERROR;
    }
    if (!(profile->tfer->buffer))
    {
        twc_tfer_load(profile);
    }
    struct t_twc_tfer_file *file = twc_tfer_file_new(
        profile, recipient, filename, friend_number, file_number, st.st_size,
        TWC_TFER_FILE_TYPE_UPLOADING);
    if (!file)
    {
        weechat_printf(profile->buffer, "%scannot open the file \"%s\"",
                       weechat_prefix("error"), filename);
        return WEECHAT_RC_ERROR;
    }
    twc_tfer_file_add(profile->tfer, file);
    twc_tfer_buffer_update(profile->tfer);
    twc_tfer_update_status(profile->tfer, "waiting for action");

    return WEECHAT_RC_OK;
}

/**
 * Custom completion for nicknames in groups.
 */
int
twc_input_complete(const void *pointer, void *data, struct t_gui_buffer *buffer,
                   const char *command)
{
    struct t_twc_chat *chat = twc_chat_search_buffer(buffer);
    if (chat && chat->group_number >= 0)
    {
        const char *input = weechat_buffer_get_string(buffer, "input");
        if (!strcmp(input, ""))
            return WEECHAT_RC_OK;
        size_t last_search_len =
            chat->last_search ? strlen(chat->last_search) : 0;
        int cmp = strncmp(chat->last_search, input, last_search_len);
        if (cmp || !last_search_len)
        {
            free(chat->last_search);
            chat->last_search = strdup(input);
            free(chat->prev_comp);
            chat->prev_comp = NULL;
            twc_starts_with(chat->nicks, chat->last_search, chat->completion);
        }

        const char *comp =
            twc_get_next_completion(chat->completion, chat->prev_comp);
        if (!comp)
            return WEECHAT_RC_OK;

        weechat_buffer_set(buffer, "completion_freeze", "1");

        char *terminator = ": "; /* Probably put it in a config */
        char temp[strlen(comp) + strlen(terminator) + 1];
        sprintf(temp, "%s%s", comp, terminator);
        weechat_buffer_set(buffer, "input", temp);

        int input_pos = weechat_buffer_get_integer(buffer, "input_length");
        int input_pos_str_size = snprintf(NULL, 0, "%d", input_pos);
        char input_pos_str[input_pos_str_size + 1];
        snprintf(input_pos_str, input_pos_str_size + 1, "%d", input_pos);
        weechat_buffer_set(buffer, "input_pos", input_pos_str);

        weechat_buffer_set(buffer, "completion_freeze", "0");

        free(chat->prev_comp);
        chat->prev_comp = strdup(comp);
        return WEECHAT_RC_OK_EAT;
    }
    return WEECHAT_RC_OK;
}
/**
 * Update a certain nick's prefix in all opened group chats
 */
void
twc_name_prefix_in_groupchats(struct t_twc_profile *profile, const char *id,
                              const char *prefix, const char *prefix_color)
{
    struct t_twc_list_item *item;
    struct t_twc_chat *chat;
    size_t index;
    twc_list_foreach (profile->chats, index, item)
    {
        chat = (struct t_twc_chat *)(item->chat);
        if ((chat->group_number) >= 0)
        {
            twc_chat_update_prefix(chat, id, prefix, prefix_color);
        }
    }
}

/**
 *  Command /ignore callback.
 */
int
twc_cmd_ignore(const void *pointer, void *data, struct t_gui_buffer *buffer,
               int argc, char **argv, char **argv_eol)
{
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    /* list ignores */
    if ((argc == 1) ||
        ((argc == 2) && (weechat_strcasecmp(argv[1], "list") == 0)))
    {
        if (!weechat_list_size(profile->ignores))
        {
            weechat_printf(profile->buffer, "ignore list is empty");
            return WEECHAT_RC_OK;
        }
        weechat_printf(profile->buffer, "ignore list:");
        size_t i = 0;
        struct t_weelist_item *item;
        for (item = weechat_list_get(profile->ignores, 0); item;
             item = weechat_list_next(item))
        {
            weechat_printf(profile->buffer, "  [%d] %s", i,
                           weechat_list_string(item));
            i++;
        }
        return WEECHAT_RC_OK;
    }
    /* add ignore */

    const char *id = argv_eol[2];
    struct t_weelist_item *item;
    item = twc_is_id_ignored(profile, id);

    if (weechat_strcasecmp(argv[1], "add") == 0)
    {
        WEECHAT_COMMAND_MIN_ARGS(3, "add");
        if (!item)
        {
            weechat_list_add(profile->ignores, id, WEECHAT_LIST_POS_END, NULL);
            weechat_printf(profile->buffer,
                           "%sID '%s' has been added to the ignore list",
                           weechat_prefix("action"), id);
            twc_name_prefix_in_groupchats(profile, id, "-", "yellow");
        }
        else
            weechat_printf(profile->buffer,
                           "%sID '%s' is already in the ignore list",
                           weechat_prefix("error"), id);
        return WEECHAT_RC_OK;
    }

    /* delete ignore */
    if (weechat_strcasecmp(argv[1], "del") == 0)
    {
        WEECHAT_COMMAND_MIN_ARGS(3, "del");
        if (item)
        {
            weechat_list_remove(profile->ignores, item);
            weechat_printf(profile->buffer,
                           "%sID '%s' has been deleted from the ignore list",
                           weechat_prefix("action"), id);
            twc_name_prefix_in_groupchats(profile, id, " ", "default");
        }
        else
            weechat_printf(profile->buffer,
                           "%sthere's no ID '%s' in the ignore list",
                           weechat_prefix("error"), id);
        return WEECHAT_RC_OK;
    }
    return WEECHAT_RC_ERROR;
}

/**
 * Register Tox-WeeChat commands.
 */
void
twc_commands_init()
{
    weechat_hook_command("bootstrap", "manage bootstrap nodes",
                         "connect <address> <port> <Tox ID>",
                         "address: internet address of node to bootstrap with\n"
                         "   port: port of the node\n"
                         " Tox ID: Tox ID of the node",
                         "connect", twc_cmd_bootstrap, NULL, NULL);

    weechat_hook_command("friend", "manage friends",
                         "list"
                         " || add [-force] <address> [<message>]"
                         " || remove <number>|<name>|<Tox ID>"
                         " || requests"
                         " || accept <number>|<Tox ID>|all"
                         " || decline <number>|<Tox ID>|all",
                         "    list: list all friends\n"
                         "     add: add a friend by their public Tox address\n"
                         "requests: list friend requests\n"
                         "  accept: accept friend requests\n"
                         " decline: decline friend requests\n",
                         "list"
                         " || add"
                         " || remove %(tox_friend_name)|%(tox_friend_tox_id)"
                         " || requests"
                         " || accept"
                         " || decline",
                         twc_cmd_friend, NULL, NULL);

    weechat_hook_command("group", "manage group chats",
                         "create"
                         " || invites"
                         " || join <number>"
                         " || decline <number>",
                         " create: create a new group chat\n"
                         "invites: list group chat invites\n"
                         "   join: join a group chat by its invite ID\n"
                         "decline: decline a group chat invite\n",
                         "create"
                         " || invites"
                         " || join",
                         twc_cmd_group, NULL, NULL);

    weechat_hook_command(
        "invite", "invite someone to a group chat", "<number>|<name>|<Tox ID>",
        "number, name, Tox ID: friend to message\n",
        "%(tox_friend_name)|%(tox_friend_tox_id)", twc_cmd_invite, NULL, NULL);

    weechat_hook_command("me", "send an action to the current chat",
                         "<message>", "message: message to send", NULL,
                         twc_cmd_me, NULL, NULL);

    weechat_hook_command("msg", "send a message to a Tox friend",
                         "<number>|<name>|<Tox ID> [<message>]",
                         "number, name, Tox ID: friend to message\n"
                         "message: message to send",
                         "%(tox_friend_name)|%(tox_friend_tox_id)", twc_cmd_msg,
                         NULL, NULL);

    weechat_hook_command("query", "send a message to a Tox friend",
                         "<number>|<name>|<Tox ID> [<message>]",
                         "number, name, Tox ID: friend to message\n"
                         "message: message to send",
                         "%(tox_friend_name)|%(tox_friend_tox_id)", twc_cmd_msg,
                         NULL, NULL);

    weechat_hook_command("myid", "get your Tox ID to give to friends", "", "",
                         NULL, twc_cmd_myid, NULL, NULL);

    weechat_hook_command("name", "change your Tox name", "<name>",
                         "name: your new name", NULL, twc_cmd_name, NULL, NULL);

    weechat_hook_command("names", "list names in a group chat", "", "", NULL,
                         twc_cmd_names, NULL, NULL);

    weechat_hook_command("nospam", "change nospam value", "[<hex value>]",
                         "hex value: new nospam value; when omitted, a random "
                         "new value is used\n\n"
                         "Warning: changing your nospam value will alter your "
                         "Tox ID!",
                         NULL, twc_cmd_nospam, NULL, NULL);

    weechat_hook_command("part", "leave a group chat", "", "", NULL,
                         twc_cmd_part, NULL, NULL);

    weechat_hook_command_run("/save", twc_cmd_save, NULL, NULL);

    weechat_hook_command_run("/input complete_next", twc_input_complete, NULL,
                             NULL);

    weechat_hook_command("status", "change your Tox status", "online|busy|away",
                         "", NULL, twc_cmd_status, NULL, NULL);

    weechat_hook_command("statusmsg", "change your Tox status message",
                         "[<message>]", "message: your new status message",
                         NULL, twc_cmd_statusmsg, NULL, NULL);

    weechat_hook_command("topic", "set a group chat topic", "<topic>",
                         "topic: new group chat topic", NULL, twc_cmd_topic,
                         NULL, NULL);

    weechat_hook_command(
        "tox", "manage Tox profiles",
        "list"
        " || create <name>"
        " || delete <name> -yes|-keepdata"
        " || load [<name>...]"
        " || unload [<name>...]"
        " || reload [<name>...]",
        "  list: list all Tox profile\n"
        "create: create a new Tox profile\n"
        "delete: delete a Tox profile; requires either -yes "
        "to confirm deletion or -keepdata to delete the "
        "profile but keep the Tox data file\n"
        "  load: load one or more Tox profiles and connect to the network\n"
        "unload: unload one or more Tox profiles\n"
        "reload: reload one or more Tox profiles\n",
        "list"
        " || create"
        " || delete %(tox_profiles) -yes|-keepdata"
        " || load %(tox_unloaded_profiles)|%*"
        " || unload %(tox_loaded_profiles)|%*"
        " || reload %(tox_loaded_profiles)|%*",
        twc_cmd_tox, NULL, NULL);
    weechat_hook_command(
        "send", "send a file to a friend",
        "<file>"
        " || <number>|<name>|<Tox ID> <file>",
        "file: path to the file\n"
        "number, name, Tox ID: the friend you are sending the file to\n",
        "%(filename)"
        " || %(tox_friend_name)|%(tox_friend_tox_id) %(filename)",
        twc_cmd_send, NULL, NULL);
    weechat_hook_command("ignore",
                         "ommit messages from people with certain Tox IDs",
                         "list"
                         " || add <ID part>"
                         " || del <ID part>",
                         "list: show the list of ignores\n"
                         "add: add an ID to the list\n"
                         "del: delete an ID from the list\n",
                         "list"
                         " || add %*"
                         " || del %*",
                         twc_cmd_ignore, NULL, NULL);
}
