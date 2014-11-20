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
#include "twc-list.h"
#include "twc-profile.h"
#include "twc-chat.h"
#include "twc-friend-request.h"
#include "twc-group-invite.h"
#include "twc-bootstrap.h"
#include "twc-sqlite.h"
#include "twc-config.h"
#include "twc-utils.h"

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
#define TWC_CHECK_PROFILE(profile)                                            \
    if (!profile)                                                             \
    {                                                                         \
        weechat_printf(NULL,                                                  \
                       "%s%s: command \"%s\" must be executed on a Tox "      \
                       "buffer",                                              \
                       weechat_prefix("error"), weechat_plugin->name,         \
                       argv[0]);                                              \
        return WEECHAT_RC_OK;                                                 \
    }                                                                         \

/**
 * Make sure a command is executed in a chat buffer. If not, warn user and
 * abort.
 */
#define TWC_CHECK_CHAT(chat)                                                  \
    if (!chat)                                                                \
    {                                                                         \
        weechat_printf(NULL,                                                  \
                       "%s%s: command \"%s\" must be executed in a chat "     \
                       "buffer",                                              \
                       weechat_prefix("error"),                               \
                       weechat_plugin->name,                                  \
                       argv[0]);                                              \
        return WEECHAT_RC_OK;                                                 \
    }

/**
 * Make sure a command is executed in a group chat buffer. If not, warn user
 * and abort.
 */
#define TWC_CHECK_GROUP_CHAT(chat)                                            \
    if (!chat || chat->group_number < 0)                                      \
    {                                                                         \
        weechat_printf(NULL,                                                  \
                       "%s%s: command \"%s\" must be executed in a group "    \
                       "chat buffer ",                                        \
                       weechat_prefix("error"),                               \
                       weechat_plugin->name,                                  \
                       argv[0]);                                              \
        return WEECHAT_RC_OK;                                                 \
    }

/**
 * Make sure a profile with the given name exists. If not, warn user and abort.
 */
#define TWC_CHECK_PROFILE_EXISTS(profile)                                     \
    if (!profile)                                                             \
    {                                                                         \
        weechat_printf(NULL,                                                  \
                       "%s%s: profile \"%s\" does not exist.",                \
                       weechat_prefix("error"), weechat_plugin->name,         \
                       name);                                                 \
        return WEECHAT_RC_OK;                                                 \
    }

/**
 * Make sure a profile is loaded.
 */
#define TWC_CHECK_PROFILE_LOADED(profile)                                     \
    if (!(profile->tox))                                                      \
    {                                                                         \
        weechat_printf(profile->buffer,                                       \
                       "%sprofile must be loaded for command \"%s\"",         \
                       weechat_prefix("error"), argv[0]);                     \
        return WEECHAT_RC_OK;                                                 \
    }

/**
 * Make sure friend exists.
 */
#define TWC_CHECK_FRIEND_NUMBER(profile, number, string)                      \
    if (number == TWC_FRIEND_MATCH_NOMATCH)                                   \
    {                                                                         \
        weechat_printf(profile->buffer,                                       \
                       "%sno friend number, name or Tox ID found matching "   \
                       "\"%s\"",                                              \
                       weechat_prefix("error"), string);                      \
        return WEECHAT_RC_OK;                                                 \
    }                                                                         \
    if (number == TWC_FRIEND_MATCH_AMBIGUOUS)                                 \
    {                                                                         \
        weechat_printf(profile->buffer,                                       \
                       "%smultiple friends with name  \"%s\" found; please "  \
                       "use Tox ID instead",                                  \
                       weechat_prefix("error"), string);                      \
        return WEECHAT_RC_OK;                                                 \
    }


/**
 * Get number of friend matching string. Tries to match number, name and
 * Tox ID.
 */
enum TWC_FRIEND_MATCH
twc_match_friend(struct t_twc_profile *profile, const char *search_string)
{
    uint32_t friend_count = tox_count_friendlist(profile->tox);
    int32_t *friend_numbers = malloc(sizeof(int32_t) * friend_count);
    tox_get_friendlist(profile->tox, friend_numbers, friend_count);

    int32_t match = TWC_FRIEND_MATCH_NOMATCH;

    char *endptr;
    unsigned long friend_number = strtoul(search_string, &endptr, 10);
    if (endptr == search_string + strlen(search_string)
        && tox_friend_exists(profile->tox, friend_number))
        return friend_number;

    size_t search_size = strlen(search_string);
    for (uint32_t i = 0; i < friend_count; ++i)
    {
        if (search_size == TOX_CLIENT_ID_SIZE * 2)
        {
            uint8_t tox_id[TOX_CLIENT_ID_SIZE];
            char hex_id[TOX_CLIENT_ID_SIZE * 2 + 1];

            tox_get_client_id(profile->tox, friend_numbers[i], tox_id);
            twc_bin2hex(tox_id, TOX_CLIENT_ID_SIZE, hex_id);

            if (weechat_strcasecmp(hex_id, search_string) == 0)
                return friend_numbers[i];
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
twc_cmd_bootstrap(void *data, struct t_gui_buffer *buffer,
                  int argc, char **argv, char **argv_eol)
{
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    // /bootstrap connect <address> <port> <key>
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
twc_cmd_friend(void *data, struct t_gui_buffer *buffer,
               int argc, char **argv, char **argv_eol)
{
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    // /friend or /friend list
    if (argc == 1 || (argc == 2 && weechat_strcasecmp(argv[1], "list") == 0))
    {
        size_t friend_count = tox_count_friendlist(profile->tox);
        int32_t friend_numbers[friend_count];
        tox_get_friendlist(profile->tox, friend_numbers, friend_count);

        if (friend_count == 0)
        {
            weechat_printf(profile->buffer,
                           "%sYou have no friends :(",
                           weechat_prefix("network"));
            return WEECHAT_RC_OK;
        }

        weechat_printf(profile->buffer,
                       "%s[#] Name [Tox ID (short)]",
                       weechat_prefix("network"));

        for (size_t i = 0; i < friend_count; ++i)
        {
            int32_t friend_number = friend_numbers[i];
            char *name = twc_get_name_nt(profile->tox, friend_number);
            char *hex_address = twc_get_friend_id_short(profile->tox,
                                                        friend_number);

            weechat_printf(profile->buffer,
                           "%s[%d] %s [%s]",
                           weechat_prefix("network"),
                           friend_number, name, hex_address);

            free(name);
            free(hex_address);
        }

        return WEECHAT_RC_OK;
    }

    // /friend add [-force] <Tox ID> [<message>]
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

        if (strlen(hex_id) != TOX_FRIEND_ADDRESS_SIZE * 2)
        {
            weechat_printf(profile->buffer,
                           "%sTox ID length invalid. Please try again.",
                           weechat_prefix("error"));

            return WEECHAT_RC_OK;
        }

        uint8_t address[TOX_FRIEND_ADDRESS_SIZE];
        twc_hex2bin(hex_id, TOX_FRIEND_ADDRESS_SIZE, address);

        if (force)
        {
            bool fail = false;
            char *hex_key = strndup(hex_id, TOX_CLIENT_ID_SIZE * 2);
            int32_t friend_number = twc_match_friend(profile, hex_key);
            free(hex_key);

            if (friend_number == TWC_FRIEND_MATCH_AMBIGUOUS)
                fail = true;
            else if (friend_number != TWC_FRIEND_MATCH_NOMATCH)
                fail = tox_del_friend(profile->tox, friend_number) != 0;

            if (fail)
            {
                weechat_printf(profile->buffer,
                               "%scould not remove friend; please remove "
                               "manually before resending friend request",
                               weechat_prefix("error"));
                return WEECHAT_RC_OK;
            }
        }

        int32_t result = tox_add_friend(profile->tox,
                                        (uint8_t *)address,
                                        (uint8_t *)message,
                                        strlen(message));

        switch (result)
        {
            case TOX_FAERR_TOOLONG:
                weechat_printf(profile->buffer,
                               "%sFriend request message too long! Try again.",
                               weechat_prefix("error"));
                break;
            case TOX_FAERR_ALREADYSENT:
                weechat_printf(profile->buffer,
                               "%sYou have already sent a friend request to "
                               "that address (use -force to circumvent)",
                               weechat_prefix("error"));
                break;
            case TOX_FAERR_OWNKEY:
                weechat_printf(profile->buffer,
                               "%sYou can't add yourself as a friend.",
                               weechat_prefix("error"));
                break;
            case TOX_FAERR_BADCHECKSUM:
                weechat_printf(profile->buffer,
                               "%sInvalid friend address - try again.",
                               weechat_prefix("error"));
                break;
            case TOX_FAERR_NOMEM:
                weechat_printf(profile->buffer,
                               "%sCould not add friend (out of memory).",
                               weechat_prefix("error"));
                break;
            case TOX_FAERR_UNKNOWN:
            case TOX_FAERR_SETNEWNOSPAM:
            case TOX_FAERR_NOMESSAGE:
                weechat_printf(profile->buffer,
                               "%sCould not add friend (unknown error).",
                               weechat_prefix("error"));
                break;
            default:
                weechat_printf(profile->buffer,
                               "%sFriend request sent!",
                               weechat_prefix("network"));
                break;
        }

        return WEECHAT_RC_OK;
    }

    // /friend remove
    else if (argc >= 3 && (weechat_strcasecmp(argv[1], "remove") == 0))
    {
        int32_t friend_number = twc_match_friend(profile, argv[2]);
        TWC_CHECK_FRIEND_NUMBER(profile, friend_number, argv[2]);

        char *name = twc_get_name_nt(profile->tox, friend_number);
        if (tox_del_friend(profile->tox, friend_number) == 0)
        {
            weechat_printf(profile->buffer,
                           "%sRemoved %s from friend list.",
                           weechat_prefix("network"), name);
        }
        else
        {
            weechat_printf(profile->buffer,
                           "%sCould not remove friend!",
                           weechat_prefix("error"));
        }

        free(name);

        return WEECHAT_RC_OK;
    }

    // friend accept|decline <number>|all
    else if (argc == 3 &&
            (weechat_strcasecmp(argv[1], "accept") == 0
             || weechat_strcasecmp(argv[1], "decline") == 0))
    {
        int accept = weechat_strcasecmp(argv[1], "accept") == 0;

        struct t_twc_friend_request *request;
        if (weechat_strcasecmp(argv[2], "all") == 0)
        {
            struct t_twc_list *requests = twc_sqlite_friend_requests(profile);
            size_t index;
            struct t_twc_list_item *item;
            twc_list_foreach(requests, index, item)
            {
                if (accept)
                    twc_friend_request_accept(item->friend_request);
                else
                    twc_friend_request_remove(item->friend_request);
            }

            weechat_printf(profile->buffer,
                           "%s%s %d friend requests.",
                           weechat_prefix("network"),
                           accept ? "Accepted" : "Declined",
                           index);

            return WEECHAT_RC_OK;
        }
        else
        {
            char *endptr;
            unsigned long num = strtoul(argv[2], &endptr, 10);
            if (endptr == argv[2] || (request = twc_friend_request_with_index(profile, num)) == NULL)
            {
                weechat_printf(profile->buffer,
                               "%sInvalid friend request ID.",
                               weechat_prefix("error"));
                return WEECHAT_RC_OK;
            }

            char hex_address[TOX_CLIENT_ID_SIZE * 2 + 1];
            twc_bin2hex(request->tox_id,
                                 TOX_CLIENT_ID_SIZE,
                                 hex_address);

            if (accept)
                twc_friend_request_accept(request);
            else
                twc_friend_request_remove(request);

            twc_friend_request_free(request);

            weechat_printf(profile->buffer,
                           "%s%s friend request from %s.",
                           weechat_prefix("network"),
                           accept ? "Accepted" : "Declined",
                           hex_address);

            return WEECHAT_RC_OK;
        }
    }

    // /friend requests
    else if (argc == 2 && weechat_strcasecmp(argv[1], "requests") == 0)
    {
        weechat_printf(profile->buffer,
                       "%sPending friend requests:",
                       weechat_prefix("network"));

        struct t_twc_list *friend_requests = twc_sqlite_friend_requests(profile);

        size_t index;
        struct t_twc_list_item *item;
        twc_list_foreach(friend_requests, index, item)
        {
            size_t short_id_length = weechat_config_integer(twc_config_short_id_size);
            char hex_address[short_id_length + 1];
            twc_bin2hex(item->friend_request->tox_id,
                        short_id_length / 2,
                        hex_address);

            weechat_printf(profile->buffer,
                           "%s[%d] Address: %s\n"
                           "[%d] Message: %s",
                           weechat_prefix("network"),
                           item->friend_request->request_id,
                           hex_address,
                           item->friend_request->request_id,
                           item->friend_request->message);
        }

        twc_friend_request_free_list(friend_requests);

        return WEECHAT_RC_OK;
    }

    return WEECHAT_RC_ERROR;
}

/**
 * Command /group callback.
 */
int
twc_cmd_group(void *data, struct t_gui_buffer *buffer,
              int argc, char **argv, char **argv_eol)
{
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    // /group create
    if (argc == 2 && weechat_strcasecmp(argv[1], "create") == 0)
    {
        int rc = tox_add_groupchat(profile->tox);
        if (rc >= 0)
            twc_chat_search_group(profile, rc, true);
        else
            weechat_printf(profile->buffer,
                           "%sCould not create group chat (unknown error)",
                           weechat_prefix("error"));

        return WEECHAT_RC_OK;
    }

    // /group join|decline <number>
    else if (argc == 3 &&
            (weechat_strcasecmp(argv[1], "join") == 0
             || weechat_strcasecmp(argv[1], "decline") == 0))
    {
        bool join = weechat_strcasecmp(argv[1], "join") == 0;

        struct t_twc_group_chat_invite *invite;

        char *endptr;
        unsigned long num = strtoul(argv[2], &endptr, 10);
        if (endptr == argv[2] || (invite = twc_group_chat_invite_with_index(profile, num)) == NULL)
        {
            weechat_printf(profile->buffer,
                           "%sInvalid group chat invite ID.",
                           weechat_prefix("error"));
            return WEECHAT_RC_OK;
        }

        if (join)
        {
            int group_number = twc_group_chat_invite_join(invite);

            // create a buffer for the new group chat
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

    // /group invites
    else if (argc == 2 && weechat_strcasecmp(argv[1], "invites") == 0)
    {
        weechat_printf(profile->buffer,
                       "%sPending group chat invites:",
                       weechat_prefix("network"));

        size_t index;
        struct t_twc_list_item *item;
        twc_list_foreach(profile->group_chat_invites, index, item)
        {
            char *friend_name =
                twc_get_name_nt(profile->tox, item->group_chat_invite->friend_number);
            weechat_printf(profile->buffer,
                           "%s[%d] From: %s",
                           weechat_prefix("network"),
                           index, friend_name);
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
twc_cmd_invite(void *data, struct t_gui_buffer *buffer,
               int argc, char **argv, char **argv_eol)
{
    if (argc == 1)
        return WEECHAT_RC_ERROR;


    struct t_twc_chat *chat = twc_chat_search_buffer(buffer);
    TWC_CHECK_PROFILE_LOADED(chat->profile);
    TWC_CHECK_GROUP_CHAT(chat);

    int32_t friend_number = twc_match_friend(chat->profile, argv_eol[1]);
    TWC_CHECK_FRIEND_NUMBER(chat->profile, friend_number, argv_eol[1]);

    int rc = tox_invite_friend(chat->profile->tox,
                               friend_number, chat->group_number);

    if (rc == 0)
    {
        char *friend_name = twc_get_name_nt(chat->profile->tox, friend_number);
        weechat_printf(chat->buffer, "%sInvited %s to the group chat.",
                       weechat_prefix("network"), friend_name);
       free(friend_name);
    }
    else
    {
        weechat_printf(chat->buffer,
                       "%sFailed to send group chat invite (unknown error)",
                       weechat_prefix("error"));
    }

    return WEECHAT_RC_OK;
}

/**
 * Command /me callback.
 */
int
twc_cmd_me(void *data, struct t_gui_buffer *buffer,
           int argc, char **argv, char **argv_eol)
{
    if (argc == 1)
        return WEECHAT_RC_ERROR;

    struct t_twc_chat *chat = twc_chat_search_buffer(buffer);
    TWC_CHECK_CHAT(chat);
    TWC_CHECK_PROFILE_LOADED(chat->profile);

    twc_chat_send_message(chat, argv_eol[1], TWC_MESSAGE_TYPE_ACTION);

    return WEECHAT_RC_OK;
}
/**
 * Command /msg callback.
 */
int
twc_cmd_msg(void *data, struct t_gui_buffer *buffer,
            int argc, char **argv, char **argv_eol)
{
    if (argc == 1)
        return WEECHAT_RC_ERROR;

    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    // do a shell split in case a friend has spaces in his name and we need
    // quotes
    int shell_argc;
    char **shell_argv = weechat_string_split_shell(argv_eol[0], &shell_argc);

    char recipient[TOX_MAX_NAME_LENGTH + 1];
    snprintf(recipient, TOX_MAX_NAME_LENGTH, "%s", shell_argv[1]);
    weechat_string_free_split(shell_argv);

    char *message = NULL;
    if (shell_argc >= 3)
    {
        // extract message, add two if quotes are used
        message = argv_eol[1] + strlen(recipient);
        if (*argv[1] == '"' || *argv[1] == '\'')
            message += 2;
    }

    int32_t friend_number = twc_match_friend(profile, recipient);
    TWC_CHECK_FRIEND_NUMBER(profile, friend_number, recipient);

    // create chat buffer if it does not exist
    struct t_twc_chat *chat = twc_chat_search_friend(profile, friend_number, true);

    // send a message if provided
    if (message)
        twc_chat_send_message(chat,
                              weechat_string_strip(message, 1, 1, " "),
                              TWC_MESSAGE_TYPE_MESSAGE);

    return WEECHAT_RC_OK;
}

/**
 * Command /myid callback.
 */
int
twc_cmd_myid(void *data, struct t_gui_buffer *buffer,
             int argc, char **argv, char **argv_eol)
{
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    uint8_t address[TOX_FRIEND_ADDRESS_SIZE];
    tox_get_address(profile->tox, address);

    char address_str[TOX_FRIEND_ADDRESS_SIZE * 2 + 1];
    twc_bin2hex(address, TOX_FRIEND_ADDRESS_SIZE, address_str);

    weechat_printf(profile->buffer,
                   "%sYour Tox address: %s",
                   weechat_prefix("network"),
                   address_str);

    return WEECHAT_RC_OK;
}

/**
 * Command /name callback.
 */
int
twc_cmd_name(void *data, struct t_gui_buffer *buffer,
             int argc, char **argv, char **argv_eol)
{
    if (argc == 1)
        return WEECHAT_RC_ERROR;

    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    char *name = argv_eol[1];

    int result = tox_set_name(profile->tox, (uint8_t *)name, strlen(name));
    if (result == -1)
    {
        weechat_printf(profile->buffer,
                       "%s%s",
                       weechat_prefix("error"),
                       "Could not change name.");
        return WEECHAT_RC_OK;
    }

    weechat_bar_item_update("input_prompt");

    weechat_printf(profile->buffer,
                   "%sYou are now known as %s",
                   weechat_prefix("network"),
                   name);

    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach(profile->chats, index, item)
    {
        weechat_printf(item->chat->buffer,
                       "%sYou are now known as %s",
                       weechat_prefix("network"),
                       name);
    }

    return WEECHAT_RC_OK;
}

/**
 * Command /nospam callback.
 */
int
twc_cmd_nospam(void *data, struct t_gui_buffer *buffer,
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

        // reverse the value bytes so it's displayed as entered in the Tox ID
        new_nospam = twc_uint32_reverse_bytes(value);
    }
    else
    {
        new_nospam = random();
    }

    uint32_t old_nospam = tox_get_nospam(profile->tox);
    tox_set_nospam(profile->tox, new_nospam);

    weechat_printf(profile->buffer,
                   "%snew nospam has been set; this changes your Tox ID! To "
                   "revert, run \"/nospam %x\"",
                   weechat_prefix("network"), twc_uint32_reverse_bytes(old_nospam));

    return WEECHAT_RC_OK;
}

int
twc_cmd_save(void *data, struct t_gui_buffer *buffer, const char *command)
{
    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach(twc_profiles, index, item)
    {
        if (!(item->profile->tox)) continue;

        int rc = twc_profile_save_data_file(item->profile);
        if (rc == -1)
        {
            weechat_printf(NULL,
                           "%s%s: failed to save data for profile %s",
                           weechat_prefix("error"), weechat_plugin->name,
                           item->profile->name);
        }
    }

    weechat_printf(NULL,
                   "%s: profile data saved",
                   weechat_plugin->name);

    return WEECHAT_RC_OK;
}

/**
 * Command /status callback.
 */
int
twc_cmd_status(void *data, struct t_gui_buffer *buffer,
               int argc, char **argv, char **argv_eol)
{
    if (argc != 2)
        return WEECHAT_RC_ERROR;

    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    TOX_USERSTATUS status = TOX_USERSTATUS_INVALID;
    if (weechat_strcasecmp(argv[1], "online") == 0)
        status = TOX_USERSTATUS_NONE;
    else if (weechat_strcasecmp(argv[1], "busy") == 0)
        status = TOX_USERSTATUS_BUSY;
    else if (weechat_strcasecmp(argv[1], "away") == 0)
        status = TOX_USERSTATUS_AWAY;

    if (status == TOX_USERSTATUS_INVALID)
        return WEECHAT_RC_ERROR;

    tox_set_user_status(profile->tox, status);
    weechat_bar_item_update("away");

    return WEECHAT_RC_OK;
}

/**
 * Command /statusmsg callback.
 */
int
twc_cmd_statusmsg(void *data, struct t_gui_buffer *buffer,
                  int argc, char **argv, char **argv_eol)
{
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
    TWC_CHECK_PROFILE(profile);
    TWC_CHECK_PROFILE_LOADED(profile);

    char *message = argc > 1 ? argv_eol[1] : " ";

    int result = tox_set_status_message(profile->tox,
                                        (uint8_t *)message,
                                        strlen(message));
    if (result == -1)
    {
        weechat_printf(profile->buffer,
                       "%s%s",
                       weechat_prefix("error"),
                       "Could not set status message.");
    }

    return WEECHAT_RC_OK;
}

/**
 * Command /topic callback.
 */
int
twc_cmd_topic(void *data, struct t_gui_buffer *buffer,
              int argc, char **argv, char **argv_eol)
{
    if (argc == 1)
        return WEECHAT_RC_ERROR;

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

    char *topic = argv_eol[1];

    int result = tox_group_set_title(chat->profile->tox, chat->group_number,
                                     (uint8_t *)topic, strlen(topic));
    if (result == -1)
    {
        weechat_printf(chat->buffer, "%s%s",
                       weechat_prefix("error"), "Could not set topic.");
        return WEECHAT_RC_OK;
    }

    twc_chat_queue_refresh(chat);

    return WEECHAT_RC_OK;
}

/**
 * Command /tox callback.
 */
int
twc_cmd_tox(void *data, struct t_gui_buffer *buffer,
                    int argc, char **argv, char **argv_eol)
{
    // /tox [list]
    if (argc == 1 || (argc == 2 && weechat_strcasecmp(argv[1], "list") == 0))
    {
        weechat_printf(NULL,
                       "%sAll Tox profiles:",
                       weechat_prefix("network"));
        size_t index;
        struct t_twc_list_item *item;
        twc_list_foreach(twc_profiles, index, item)
        {
            weechat_printf(NULL,
                           "%s%s",
                           weechat_prefix("network"),
                           item->profile->name);
        }

        return WEECHAT_RC_OK;
    }

    // /tox create
    else if (argc == 3 && (weechat_strcasecmp(argv[1], "create") == 0))
    {
        char *name = argv[2];

        if (twc_profile_search_name(name))
        {
            weechat_printf(NULL,
                           "%s%s: profile \"%s\" already exists!",
                           weechat_prefix("error"), weechat_plugin->name,
                           name);
            return WEECHAT_RC_OK;
        }

        struct t_twc_profile *profile = twc_profile_new(name);
        weechat_printf(NULL,
                       "%s%s: profile \"%s\" created!",
                       weechat_prefix("network"), weechat_plugin->name,
                       profile->name);

        return WEECHAT_RC_OK;
    }

    // /tox delete
    else if ((argc == 3 || argc == 4)
             && (weechat_strcasecmp(argv[1], "delete") == 0))
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

        weechat_printf(NULL,
                       "%s%s: profile \"%s\" has been deleted.",
                       weechat_prefix("error"), weechat_plugin->name,
                       name);

        return WEECHAT_RC_OK;
    }

    // /tox load|unload|reload [<profile>...]
    else if (argc >= 2 && (weechat_strcasecmp(argv[1], "load") == 0
                           || weechat_strcasecmp(argv[1], "unload") == 0
                           || weechat_strcasecmp(argv[1], "reload") == 0))
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
 * Register Tox-WeeChat commands.
 */
void
twc_commands_init()
{
    weechat_hook_command("bootstrap",
                         "manage bootstrap nodes",
                         "connect <address> <port> <Tox ID>",
                         "address: internet address of node to bootstrap with\n"
                         "   port: port of the node\n"
                         " Tox ID: Tox ID of the node",
                         "connect", twc_cmd_bootstrap, NULL);

    weechat_hook_command("friend",
                         "manage friends",
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
                         twc_cmd_friend, NULL);

    weechat_hook_command("group",
                         "manage group chats",
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
                         twc_cmd_group, NULL);

    weechat_hook_command("invite",
                         "invite someone to a group chat",
                         "<number>|<name>|<Tox ID>",
                         "number, name, Tox ID: friend to message\n",
                         "%(tox_friend_name)|%(tox_friend_tox_id)",
                         twc_cmd_invite, NULL);

    weechat_hook_command("me",
                         "send an action to the current chat",
                         "<message>",
                         "message: message to send",
                         NULL, twc_cmd_me, NULL);

    weechat_hook_command("msg",
                         "send a message to a Tox friend",
                         "<number>|<name>|<Tox ID> [<message>]",
                         "number, name, Tox ID: friend to message\n"
                         "message: message to send",
                         "%(tox_friend_name)|%(tox_friend_tox_id)",
                         twc_cmd_msg, NULL);

    weechat_hook_command("myid",
                         "get your Tox ID to give to friends",
                         "", "",
                         NULL, twc_cmd_myid, NULL);

    weechat_hook_command("name",
                         "change your Tox name",
                         "<name>",
                         "name: your new name",
                         NULL, twc_cmd_name, NULL);

    weechat_hook_command("nospam",
                         "change nospam value",
                         "[<hex value>]",
                         "hex value: new nospam value; when omitted, a random "
                         "new value is used\n\n"
                         "Warning: changing your nospam value will alter your "
                         "Tox ID!",
                         NULL, twc_cmd_nospam, NULL);

    weechat_hook_command_run("/save", twc_cmd_save, NULL);

    weechat_hook_command("status",
                         "change your Tox status",
                         "online|busy|away",
                         "",
                         NULL, twc_cmd_status, NULL);

    weechat_hook_command("statusmsg",
                         "change your Tox status message",
                         "[<message>]",
                         "message: your new status message",
                         NULL, twc_cmd_statusmsg, NULL);

    weechat_hook_command("topic",
                         "set a group chat topic",
                         "<topic>",
                         "topic: new group chat topic",
                         NULL, twc_cmd_topic, NULL);

    weechat_hook_command("tox",
                         "manage Tox profiles",
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
                         twc_cmd_tox, NULL);
}

