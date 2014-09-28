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

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "twc.h"
#include "twc-utils.h"
#include "twc-profile.h"
#include "twc-chat.h"
#include "twc-friend-request.h"
#include "twc-bootstrap.h"
#include "twc-list.h"

#include "twc-commands.h"

/**
 * Make sure a command is executed on a Tox profile buffer. If not, warn user
 * and abort.
 */
#define TWC_CHECK_PROFILE(profile) \
    if (!profile) \
    { \
        weechat_printf(NULL, \
                       "%s%s: command \"%s\" must be executed on a Tox buffer", \
                       weechat_prefix("error"), weechat_plugin->name, \
                       argv[0]); \
        return WEECHAT_RC_OK; \
    } \

/**
 * Make sure a command is executed in a chat buffer. If not, warn user and
 * abort.
 */
#define TWC_CHECK_CHAT(chat) \
    if (!chat) \
    { \
        weechat_printf(NULL, \
                       "%s%s: command \"%s\" must be executed in a chat buffer", \
                       weechat_prefix("error"), weechat_plugin->name, argv[0]); \
        return WEECHAT_RC_OK; \
    }

/**
 * Make sure a profile with the given name exists. If not, warn user and
 * abort.
 */
#define TWC_CHECK_PROFILE_EXISTS(profile) \
    if (!profile) \
    { \
        weechat_printf(NULL, \
                       "%s%s: profile \"%s\" does not exist.", \
                       weechat_prefix("error"), weechat_plugin->name, \
                       name); \
        return WEECHAT_RC_OK; \
    }

/**
 * Make sure a profile is loaded.
 */
#define TWC_CHECK_PROFILE_LOADED(profile) \
    if (!(profile->tox)) \
    { \
        weechat_printf(profile->buffer, \
                       "%sprofile must be loaded for command \"%s\"", \
                       weechat_prefix("error"), argv[0]); \
        return WEECHAT_RC_OK; \
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

    // /friend add <Tox ID> [<message>]
    else if (argc >= 3 && weechat_strcasecmp(argv[1], "add") == 0)
    {
        if (strlen(argv[2]) != TOX_FRIEND_ADDRESS_SIZE * 2)
        {
            weechat_printf(profile->buffer,
                           "%sTox ID length invalid. Please try again.",
                           weechat_prefix("error"));

            return WEECHAT_RC_OK;
        }

        char address[TOX_FRIEND_ADDRESS_SIZE];
        twc_hex2bin(argv[2], TOX_FRIEND_ADDRESS_SIZE, address);

        char *message;
        if (argc == 3 || strlen(argv_eol[3]) == 0)
            // TODO: default message as option
            message = "Hi! Please add me on Tox!";
        else
            message = argv_eol[3];

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
                               "%sYou have already sent a friend request to that address.",
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

    // /friend remove <number>
    else if (argc == 3 && (weechat_strcasecmp(argv[1], "remove") == 0))
    {
        char *endptr;
        unsigned long friend_number = strtoul(argv[2], &endptr, 10);

        if (endptr == argv[2] || !tox_friend_exists(profile->tox, friend_number))
        {
            weechat_printf(profile->buffer,
                           "%sInvalid friend number.",
                           weechat_prefix("error"));
            return WEECHAT_RC_OK;
        }

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
            int count = 0;
            while ((request = twc_friend_request_with_index(profile, 0)) != NULL)
            {
                if (accept)
                    twc_friend_request_accept(request);
                else
                    twc_friend_request_remove(request);

                ++count;
            }

            weechat_printf(profile->buffer,
                           "%s%s %d friend requests.",
                           weechat_prefix("network"),
                           accept ? "Accepted" : "Declined",
                           count);

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
        if (profile->friend_requests == NULL)
        {
            weechat_printf(profile->buffer,
                           "%sNo pending friend requests :(",
                           weechat_prefix("network"));
        }
        else
        {
            weechat_printf(profile->buffer,
                           "%sPending friend requests:",
                           weechat_prefix("network"));

            size_t index;
            struct t_twc_list_item *item;
            twc_list_foreach(profile->friend_requests, index, item)
            {
                // TODO: load short form address length from config
                char hex_address[12 + 1];
                twc_bin2hex(item->friend_request->tox_id,
                            6,
                            hex_address);

                weechat_printf(profile->buffer,
                               "%s[%d] Address: %s\n"
                               "[%d] Message: %s",
                               weechat_prefix("network"),
                               index, hex_address,
                               index, item->friend_request->message);
            }
        }

        return WEECHAT_RC_OK;
    }

    return WEECHAT_RC_ERROR;
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

    char *endptr;
    unsigned long friend_number = strtoul(argv[1], &endptr, 10);

    if (endptr == argv[1] || !tox_friend_exists(profile->tox, friend_number))
    {
        weechat_printf(profile->buffer,
                       "%sInvalid friend number.",
                       weechat_prefix("error"));
        return WEECHAT_RC_OK;
    }

    // create chat buffer if it does not exist
    struct t_twc_chat *chat = twc_chat_search_friend(profile, friend_number, true);

    // send a message if provided
    if (argc >= 3)
        twc_chat_send_message(chat, argv_eol[2], TWC_MESSAGE_TYPE_MESSAGE);

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

    // /tox load <profile>
    else if (argc >= 2 && (weechat_strcasecmp(argv[1], "load") == 0))
    {
        if (argc == 2)
        {
            struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
            if (!profile)
                return WEECHAT_RC_ERROR;

            twc_profile_load(profile);
        }
        else
        {
            for (int i = 2; i < argc; ++i)
            {
                char *name = argv[i];
                struct t_twc_profile *profile = twc_profile_search_name(name);
                TWC_CHECK_PROFILE_EXISTS(profile);

                twc_profile_load(profile);
            }
        }

        return WEECHAT_RC_OK;
    }

    // /tox unload <profile>
    else if (argc >= 2 && (weechat_strcasecmp(argv[1], "unload") == 0))
    {
        if (argc == 2)
        {
            struct t_twc_profile *profile = twc_profile_search_buffer(buffer);
            if (!profile)
                return WEECHAT_RC_ERROR;

            twc_profile_unload(profile);
        }
        else
        {
            for (int i = 2; i < argc; ++i)
            {
                char *name = argv[i];
                struct t_twc_profile *profile = twc_profile_search_name(name);
                TWC_CHECK_PROFILE_EXISTS(profile);

                twc_profile_unload(profile);
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
                         NULL, twc_cmd_bootstrap, NULL);

    weechat_hook_command("friend",
                         "manage friends",
                         "list"
                         " || add <address> [<message>]"
                         " || remove <number>"
                         " || requests"
                         " || accept <number>|all"
                         " || decline <number>|all",
                         "    list: list all friends\n"
                         "     add: add a friend by their public Tox address\n"
                         "requests: list friend requests\n"
                         "  accept: accept friend requests\n"
                         " decline: decline friend requests\n",
                         NULL, twc_cmd_friend, NULL);

    weechat_hook_command("me",
                         "send an action to the current chat",
                         "<message>",
                         "message: message to send",
                         NULL, twc_cmd_me, NULL);

    weechat_hook_command("msg",
                         "send a message to a Tox friend",
                         "<id> [<message>]",
                         "     id: friend number of the person to message\n"
                         "message: message to send",
                         NULL, twc_cmd_msg, NULL);

    weechat_hook_command("myid",
                         "get your Tox ID to give to friends",
                         "", "",
                         NULL, twc_cmd_myid, NULL);

    weechat_hook_command("name",
                         "change your Tox name",
                         "<name>",
                         "name: your new name",
                         NULL, twc_cmd_name, NULL);

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

    weechat_hook_command("tox",
                         "manage Tox profiles",
                         "list"
                         " || create <name>"
                         " || delete <name> -yes|-keepdata"
                         " || load [<name>...]"
                         " || unload [<name>...]",
                         "  list: list all Tox profile\n"
                         "create: create a new Tox profile\n"
                         "delete: delete a Tox profile; requires either -yes "
                         "to confirm deletion or -keepdata to delete the "
                         "profile but keep the Tox data file\n"
                         "  load: load a Tox profile and connect to the network\n"
                         "unload: unload a Tox profile\n",
                         "list"
                         " || create"
                         " || delete %(tox_profiles) -yes|-keepdata"
                         " || load %(tox_unloaded_profiles)|%*"
                         " || unload %(tox_loaded_profiles)|%*",
                         twc_cmd_tox, NULL);
}

