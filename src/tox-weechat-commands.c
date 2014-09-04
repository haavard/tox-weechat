#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"
#include "tox-weechat-tox.h"
#include "tox-weechat-utils.h"
#include "tox-weechat-chats.h"
#include "tox-weechat-friend-requests.h"

#include "tox-weechat-commands.h"

int
tox_weechat_cmd_bootstrap(void *data, struct t_gui_buffer *buffer,
                          int argc, char **argv, char **argv_eol)
{
    if (argc != 4)
        return WEECHAT_RC_ERROR;

    char *address = argv[1];
    uint16_t port = atoi(argv[2]);
    char *tox_address = argv[3];

    if (!tox_weechat_bootstrap(address, port, tox_address))
    {
        weechat_printf(tox_main_buffer,
                       "%sInvalid arguments for bootstrap.",
                       weechat_prefix("error"));
    }

    return WEECHAT_RC_OK;
}

int
tox_weechat_cmd_friend(void *data, struct t_gui_buffer *buffer,
                       int argc, char **argv, char **argv_eol)
{
    // /friend [list]
    if (argc == 1 || (argc == 2 && weechat_strcasecmp(argv[1], "list") == 0))
    {
        size_t friend_count = tox_count_friendlist(tox);
        int32_t friend_numbers[friend_count];
        tox_get_friendlist(tox, friend_numbers, friend_count);

        if (friend_count == 0)
        {
            weechat_printf(tox_main_buffer,
                           "%sYou have no friends :(",
                           weechat_prefix("network"));
            return WEECHAT_RC_OK;
        }

        weechat_printf(tox_main_buffer,
                       "%s[#] Name [client ID]",
                       weechat_prefix("network"));

        for (size_t i = 0; i < friend_count; ++i)
        {
            int32_t friend_number = friend_numbers[i];
            char *name = tox_weechat_get_name_nt(friend_number);

            uint8_t client_id[TOX_CLIENT_ID_SIZE];
            tox_get_client_id(tox, friend_number, client_id);
            char *hex_id = tox_weechat_bin2hex(client_id, TOX_CLIENT_ID_SIZE);

            weechat_printf(tox_main_buffer,
                           "%s[%d] %s [%s]",
                           weechat_prefix("network"),
                           friend_number, name, hex_id);

            free(name);
            free(hex_id);
        }

        return WEECHAT_RC_OK;
    }

    if (argc >= 3 && (weechat_strcasecmp(argv[1], "add") == 0))
    {
        uint8_t *address = tox_weechat_hex2bin(argv[2]);

        char *message;
        if (argc == 3 || strlen(argv_eol[3]) == 0)
            message = "Hi! Please add me on Tox!";
        else
            message = argv_eol[3];

        int32_t result = tox_add_friend(tox,
                                        address,
                                        (uint8_t *)message,
                                        strlen(message));

        switch (result)
        {
            case TOX_FAERR_TOOLONG:
                weechat_printf(tox_main_buffer,
                               "%sFriend request message too long! Try again.",
                               weechat_prefix("error"));
                break;
            case TOX_FAERR_ALREADYSENT:
                weechat_printf(tox_main_buffer,
                               "%sYou have already sent a friend request to that address.",
                               weechat_prefix("error"));
                break;
            case TOX_FAERR_OWNKEY:
                weechat_printf(tox_main_buffer,
                               "%sYou can't add yourself as a friend.",
                               weechat_prefix("error"));
                break;
            case TOX_FAERR_BADCHECKSUM:
                weechat_printf(tox_main_buffer,
                               "%sInvalid friend address - try again.",
                               weechat_prefix("error"));
                break;
            case TOX_FAERR_NOMEM:
                weechat_printf(tox_main_buffer,
                               "%sCould not add friend (out of memory).",
                               weechat_prefix("error"));
                break;
            case TOX_FAERR_UNKNOWN:
            case TOX_FAERR_SETNEWNOSPAM:
                weechat_printf(tox_main_buffer,
                               "%sCould not add friend (unknown error).",
                               weechat_prefix("error"));
                break;
            default:
                weechat_printf(tox_main_buffer,
                               "%sFriend request sent!",
                               weechat_prefix("network"));
                break;
        }


        return WEECHAT_RC_OK;
    }

    if (argc == 3 && (weechat_strcasecmp(argv[1], "remove") == 0))
    {
        char *endptr;
        unsigned long friend_number = strtoul(argv[2], &endptr, 10);

        if (endptr == argv[2] || !tox_friend_exists(tox, friend_number))
        {
            weechat_printf(tox_main_buffer,
                           "%sInvalid friend number.",
                           weechat_prefix("error"));
            return WEECHAT_RC_OK;
        }

        char *name = tox_weechat_get_name_nt(friend_number);
        if (tox_del_friend(tox, friend_number) == 0)
        {
            weechat_printf(tox_main_buffer,
                           "%sRemoved %s from friend list.",
                           weechat_prefix("network"), name);
        }
        else
        {
            weechat_printf(tox_main_buffer,
                           "%sCould not remove friend!",
                           weechat_prefix("error"));
        }

        return WEECHAT_RC_OK;
    }

    // /friend accept
    if (argc == 3 &&
            (weechat_strcasecmp(argv[1], "accept") == 0
             || weechat_strcasecmp(argv[1], "decline") == 0))
    {
        int accept = weechat_strcasecmp(argv[1], "accept") == 0;

        struct t_tox_friend_request *request;
        if (weechat_strcasecmp(argv[2], "all") == 0)
        {
            int count = 0;
            while ((request = tox_weechat_friend_request_with_num(1)) != NULL)
            {
                if (accept)
                    tox_weechat_accept_friend_request(request);
                else
                    tox_weechat_decline_friend_request(request);

                ++count;
            }

            weechat_printf(tox_main_buffer,
                           "%s%s %d friend requests.",
                           weechat_prefix("network"),
                           accept ? "Accepted" : "Declined",
                           count);

            return WEECHAT_RC_OK;
        }
        else
        {
            unsigned int num = atoi(argv[2]);
            if (num == 0 || (request = tox_weechat_friend_request_with_num(num)) == NULL)
            {
                weechat_printf(tox_main_buffer,
                               "%sInvalid friend request ID.",
                               weechat_prefix("error"));
                return WEECHAT_RC_OK;
            }

            char *hex_id = tox_weechat_bin2hex(request->address, TOX_CLIENT_ID_SIZE);

            if (accept)
                tox_weechat_accept_friend_request(request);
            else
                tox_weechat_decline_friend_request(request);

            weechat_printf(tox_main_buffer,
                           "%s%s friend request from %s.",
                           weechat_prefix("network"),
                           accept ? "Accepted" : "Declined",
                           hex_id);
            free(hex_id);

            return WEECHAT_RC_OK;
        }
    }

    if (argc == 2 && weechat_strcasecmp(argv[1], "requests") == 0)
    {
        struct t_tox_friend_request *request = tox_weechat_first_friend_request();
        if (request == NULL)
        {
            weechat_printf(tox_main_buffer,
                           "%sNo pending friend requests :(",
                           weechat_prefix("network"));
        }
        else
        {
            weechat_printf(tox_main_buffer,
                           "%sPending friend requests:",
                           weechat_prefix("network"));

            unsigned int num = 1;
            while (request)
            {
                char *hex_address = tox_weechat_bin2hex(request->address,
                                                        TOX_CLIENT_ID_SIZE);
                weechat_printf(tox_main_buffer,
                               "%s[%d] Address: %s\n"
                               "[%d] Message: %s",
                               weechat_prefix("network"),
                               num, hex_address,
                               num, request->message);

                free(hex_address);

                request = request->next;
                ++num;
            }
        }

        return WEECHAT_RC_OK;
    }

    return WEECHAT_RC_ERROR;
}

int
tox_weechat_cmd_me(void *data, struct t_gui_buffer *buffer,
                   int argc, char **argv, char **argv_eol)
{
    if (argc == 1)
        return WEECHAT_RC_ERROR;

    struct t_tox_chat *chat = tox_weechat_get_chat_for_buffer(buffer);

    tox_send_action(tox,
                    chat->friend_number,
                    (uint8_t *)argv_eol[1],
                    strlen(argv_eol[1]));

    char *name = tox_weechat_get_self_name_nt();
    tox_weechat_chat_print_action(chat, name, argv_eol[1]);

    free(name);

    return WEECHAT_RC_OK;
}

int
tox_weechat_cmd_msg(void *data, struct t_gui_buffer *buffer,
                    int argc, char **argv, char **argv_eol)
{
    if (argc == 1)
        return WEECHAT_RC_ERROR;

    char *endptr;
    unsigned long friend_number = strtoul(argv[1], &endptr, 10);

    if (endptr == argv[1] || !tox_friend_exists(tox, friend_number))
    {
        weechat_printf(tox_main_buffer,
                       "%sInvalid friend number.",
                       weechat_prefix("error"));
        return WEECHAT_RC_OK;
    }

    struct t_tox_chat *chat = tox_weechat_get_friend_chat(friend_number);
    if (argc >= 3)
    {
        tox_send_message(tox,
                         friend_number,
                         (uint8_t *)argv_eol[1],
                         strlen(argv_eol[2]));
        char *name = tox_weechat_get_self_name_nt();
        tox_weechat_chat_print_message(chat, name, argv_eol[1]);
        free(name);
    }

    return WEECHAT_RC_OK;
}

int
tox_weechat_cmd_myaddress(void *data, struct t_gui_buffer *buffer,
                          int argc, char **argv, char **argv_eol)
{
    uint8_t address[TOX_FRIEND_ADDRESS_SIZE];
    tox_get_address(tox, address);

    char *address_str = tox_weechat_bin2hex(address, TOX_FRIEND_ADDRESS_SIZE);

    weechat_printf(tox_main_buffer,
                   "%sYour Tox address: %s",
                   weechat_prefix("network"),
                   address_str);

    free(address_str);

    return WEECHAT_RC_OK;
}

int
tox_weechat_cmd_name(void *data, struct t_gui_buffer *buffer,
                     int argc, char **argv, char **argv_eol)
{
    if (argc == 1)
        return WEECHAT_RC_ERROR;

    char *name = argv_eol[1];

    int result = tox_set_name(tox, (uint8_t *)name, strlen(name));
    if (result == -1)
    {
        weechat_printf(tox_main_buffer,
                       "%s%s",
                       weechat_prefix("error"),
                       "Could not change name.");
        return WEECHAT_RC_OK;
    }

    weechat_bar_item_update("input_prompt");

    weechat_printf(tox_main_buffer,
                   "%sYou are now known as %s",
                   weechat_prefix("network"),
                   name);

    for (struct t_tox_chat *chat = tox_weechat_get_first_chat();
         chat;
         chat = chat->next)
    {
        weechat_printf(chat->buffer,
                       "%sYou are now known as %s",
                       weechat_prefix("network"),
                       name);
    }

    return WEECHAT_RC_OK;
}

int
tox_weechat_cmd_status(void *data, struct t_gui_buffer *buffer,
                       int argc, char **argv, char **argv_eol)
{
    if (argc != 2)
        return WEECHAT_RC_ERROR;

    TOX_USERSTATUS status = TOX_USERSTATUS_INVALID;
    if (weechat_strcasecmp(argv[1], "online") == 0)
        status = TOX_USERSTATUS_NONE;
    else if (weechat_strcasecmp(argv[1], "busy") == 0)
        status = TOX_USERSTATUS_BUSY;
    else if (weechat_strcasecmp(argv[1], "away") == 0)
        status = TOX_USERSTATUS_AWAY;

    if (status == TOX_USERSTATUS_INVALID)
        return WEECHAT_RC_ERROR;

    tox_set_user_status(tox, status);
    weechat_bar_item_update("away");

    return WEECHAT_RC_OK;
}

int
tox_weechat_cmd_statusmsg(void *data, struct t_gui_buffer *buffer,
                          int argc, char **argv, char **argv_eol)
{
    char *message = argc > 1 ? argv_eol[1] : " ";

    int result = tox_set_status_message(tox, (uint8_t *)message, strlen(message));
    if (result == -1)
    {
        weechat_printf(tox_main_buffer,
                       "%s%s",
                       weechat_prefix("error"),
                       "Could not set status message.");
    }

    return WEECHAT_RC_OK;
}

void
tox_weechat_commands_init()
{
    weechat_hook_command("bootstrap",
                         "bootstrap the Tox DHT",
                         "<address> <port> <client id>",
                         "  address: internet address of node to bootstrap with\n"
                         "     port: port of the node\n"
                         "client id: Tox client  of the node",
                         NULL, tox_weechat_cmd_bootstrap, NULL);

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
                         NULL, tox_weechat_cmd_friend, NULL);

    weechat_hook_command("me",
                         "send an action to the current chat",
                         "<message>",
                         "message: message to send",
                         NULL, tox_weechat_cmd_me, NULL);

    weechat_hook_command("msg",
                         "send a message to a Tox friend",
                         "<id> [<message>]",
                         "     id: friend number of the person to message\n"
                         "message: message to send",
                         NULL, tox_weechat_cmd_msg, NULL);

    weechat_hook_command("myaddress",
                         "get your Tox address to give to friends",
                         "", "",
                         NULL, tox_weechat_cmd_myaddress, NULL);

    weechat_hook_command("name",
                         "change your Tox name",
                         "<name>",
                         "name: your new name",
                         NULL, tox_weechat_cmd_name, NULL);

    weechat_hook_command("status",
                         "change your Tox status",
                         "online|busy|away",
                         "",
                         NULL, tox_weechat_cmd_status, NULL);

    weechat_hook_command("statusmsg",
                         "change your Tox status message",
                         "[<message>]",
                         "message: your new status message",
                         NULL, tox_weechat_cmd_statusmsg, NULL);
}
