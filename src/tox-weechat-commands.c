#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"
#include "tox-weechat-utils.h"
#include "tox-weechat-identities.h"
#include "tox-weechat-chats.h"
#include "tox-weechat-friend-requests.h"

#include "tox-weechat-commands.h"

// TODO: something
extern int
tox_weechat_bootstrap_tox(Tox *tox, char *address, uint16_t port, char *public_key);

int
tox_weechat_cmd_bootstrap(void *data, struct t_gui_buffer *buffer,
                          int argc, char **argv, char **argv_eol)
{
    struct t_tox_weechat_identity *identity = tox_weechat_identity_for_buffer(buffer);
    if (!identity)
    {
        weechat_printf(NULL,
                       "%s%s: command \"%s\" must be executed on a Tox buffer",
                       weechat_prefix("error"),
                       weechat_plugin->name,
                       argv[0]);
        return WEECHAT_RC_OK;
    }

    if (argc != 4)
        return WEECHAT_RC_ERROR;

    char *address = argv[1];
    uint16_t port = atoi(argv[2]);
    char *tox_address = argv[3];

    if (!tox_weechat_bootstrap_tox(identity->tox, address, port, tox_address))
    {
        weechat_printf(identity->buffer,
                       "%sInvalid arguments for bootstrap.",
                       weechat_prefix("error"));
    }

    return WEECHAT_RC_OK;
}

int
tox_weechat_cmd_friend(void *data, struct t_gui_buffer *buffer,
                       int argc, char **argv, char **argv_eol)
{
    struct t_tox_weechat_identity *identity = tox_weechat_identity_for_buffer(buffer);
    if (!identity)
    {
        weechat_printf(NULL,
                       "%s%s: command \"%s\" must be executed on a Tox buffer",
                       weechat_prefix("error"),
                       weechat_plugin->name,
                       argv[0]);
        return WEECHAT_RC_OK;
    }

    if (argc == 1 || (argc == 2 && weechat_strcasecmp(argv[1], "list") == 0))
    {
        size_t friend_count = tox_count_friendlist(identity->tox);
        int32_t friend_numbers[friend_count];
        tox_get_friendlist(identity->tox, friend_numbers, friend_count);

        if (friend_count == 0)
        {
            weechat_printf(identity->buffer,
                           "%sYou have no friends :(",
                           weechat_prefix("network"));
            return WEECHAT_RC_OK;
        }

        weechat_printf(identity->buffer,
                       "%s[#] Name [client ID]",
                       weechat_prefix("network"));

        for (size_t i = 0; i < friend_count; ++i)
        {
            int32_t friend_number = friend_numbers[i];
            char *name = tox_weechat_get_name_nt(identity->tox, friend_number);

            uint8_t client_id[TOX_CLIENT_ID_SIZE];
            tox_get_client_id(identity->tox, friend_number, client_id);
            char *hex_address = malloc(TOX_CLIENT_ID_SIZE * 2 + 1);
            tox_weechat_bin2hex(client_id,
                                TOX_CLIENT_ID_SIZE,
                                hex_address);

            weechat_printf(identity->buffer,
                           "%s[%d] %s [%s]",
                           weechat_prefix("network"),
                           friend_number, name, hex_address);

            free(name);
            free(hex_address);
        }

        return WEECHAT_RC_OK;
    }

    else if (argc >= 3 && (weechat_strcasecmp(argv[1], "add") == 0))
    {
        char *address = malloc(TOX_FRIEND_ADDRESS_SIZE);
        tox_weechat_hex2bin(argv[2], address);

        char *message;
        if (argc == 3 || strlen(argv_eol[3]) == 0)
            message = "Hi! Please add me on Tox!";
        else
            message = argv_eol[3];

        int32_t result = tox_add_friend(identity->tox,
                                        (uint8_t *)address,
                                        (uint8_t *)message,
                                        strlen(message));

        switch (result)
        {
            case TOX_FAERR_TOOLONG:
                weechat_printf(identity->buffer,
                               "%sFriend request message too long! Try again.",
                               weechat_prefix("error"));
                break;
            case TOX_FAERR_ALREADYSENT:
                weechat_printf(identity->buffer,
                               "%sYou have already sent a friend request to that address.",
                               weechat_prefix("error"));
                break;
            case TOX_FAERR_OWNKEY:
                weechat_printf(identity->buffer,
                               "%sYou can't add yourself as a friend.",
                               weechat_prefix("error"));
                break;
            case TOX_FAERR_BADCHECKSUM:
                weechat_printf(identity->buffer,
                               "%sInvalid friend address - try again.",
                               weechat_prefix("error"));
                break;
            case TOX_FAERR_NOMEM:
                weechat_printf(identity->buffer,
                               "%sCould not add friend (out of memory).",
                               weechat_prefix("error"));
                break;
            case TOX_FAERR_UNKNOWN:
            case TOX_FAERR_SETNEWNOSPAM:
            case TOX_FAERR_NOMESSAGE:
                weechat_printf(identity->buffer,
                               "%sCould not add friend (unknown error).",
                               weechat_prefix("error"));
                break;
            default:
                weechat_printf(identity->buffer,
                               "%sFriend request sent!",
                               weechat_prefix("network"));
                break;
        }


        return WEECHAT_RC_OK;
    }

    else if (argc == 3 && (weechat_strcasecmp(argv[1], "remove") == 0))
    {
        char *endptr;
        unsigned long friend_number = strtoul(argv[2], &endptr, 10);

        if (endptr == argv[2] || !tox_friend_exists(identity->tox, friend_number))
        {
            weechat_printf(identity->buffer,
                           "%sInvalid friend number.",
                           weechat_prefix("error"));
            return WEECHAT_RC_OK;
        }

        char *name = tox_weechat_get_name_nt(identity->tox, friend_number);
        if (tox_del_friend(identity->tox, friend_number) == 0)
        {
            weechat_printf(identity->buffer,
                           "%sRemoved %s from friend list.",
                           weechat_prefix("network"), name);
        }
        else
        {
            weechat_printf(identity->buffer,
                           "%sCould not remove friend!",
                           weechat_prefix("error"));
        }

        free(name);

        return WEECHAT_RC_OK;
    }

    else if (argc == 3 &&
            (weechat_strcasecmp(argv[1], "accept") == 0
             || weechat_strcasecmp(argv[1], "decline") == 0))
    {
        int accept = weechat_strcasecmp(argv[1], "accept") == 0;

        struct t_tox_weechat_friend_request *request;
        if (weechat_strcasecmp(argv[2], "all") == 0)
        {
            int count = 0;
            while ((request = tox_weechat_friend_request_with_num(identity, 0)) != NULL)
            {
                if (accept)
                    tox_weechat_accept_friend_request(request);
                else
                    tox_weechat_decline_friend_request(request);

                ++count;
            }

            weechat_printf(identity->buffer,
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
            if (endptr == argv[2] || (request = tox_weechat_friend_request_with_num(identity, num)) == NULL)
            {
                weechat_printf(identity->buffer,
                               "%sInvalid friend request ID.",
                               weechat_prefix("error"));
                return WEECHAT_RC_OK;
            }

            char *hex_address = malloc(TOX_CLIENT_ID_SIZE * 2 + 1);
            tox_weechat_bin2hex(request->address,
                                TOX_CLIENT_ID_SIZE,
                                hex_address);

            if (accept)
                tox_weechat_accept_friend_request(request);
            else
                tox_weechat_decline_friend_request(request);

            weechat_printf(identity->buffer,
                           "%s%s friend request from %s.",
                           weechat_prefix("network"),
                           accept ? "Accepted" : "Declined",
                           hex_address);
            free(hex_address);

            return WEECHAT_RC_OK;
        }
    }

    else if (argc == 2 && weechat_strcasecmp(argv[1], "requests") == 0)
    {
        if (identity->friend_requests == NULL)
        {
            weechat_printf(identity->buffer,
                           "%sNo pending friend requests :(",
                           weechat_prefix("network"));
        }
        else
        {
            weechat_printf(identity->buffer,
                           "%sPending friend requests:",
                           weechat_prefix("network"));

            int num = 0;
            for (struct t_tox_weechat_friend_request *request = identity->friend_requests;
                 request;
                 request = request->next_request)
            {
                char *hex_address = malloc(TOX_CLIENT_ID_SIZE * 2 + 1);
                tox_weechat_bin2hex(request->address,
                                    TOX_CLIENT_ID_SIZE,
                                    hex_address);

                weechat_printf(identity->buffer,
                               "%s[%d] Address: %s\n"
                               "[%d] Message: %s",
                               weechat_prefix("network"),
                               num, hex_address,
                               num, request->message);

                free(hex_address);

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

    struct t_tox_weechat_identity *identity = tox_weechat_identity_for_buffer(buffer);
    struct t_tox_weechat_chat *chat = tox_weechat_get_chat_for_buffer(buffer);

    if (!chat)
    {
        weechat_printf(NULL,
                       "%s%s: command \"%s\" must be executed in a chat buffer",
                       weechat_prefix("error"),
                       weechat_plugin->name,
                       argv[0]);
        return WEECHAT_RC_OK;
    }

    tox_send_action(identity->tox,
                    chat->friend_number,
                    (uint8_t *)argv_eol[1],
                    strlen(argv_eol[1]));

    char *name = tox_weechat_get_self_name_nt(identity->tox);
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

    struct t_tox_weechat_identity *identity = tox_weechat_identity_for_buffer(buffer);
    if (!identity)
    {
        weechat_printf(NULL,
                       "%s%s: command \"%s\" must be executed in a Tox buffer",
                       weechat_prefix("error"),
                       weechat_plugin->name,
                       argv[0]);
        return WEECHAT_RC_OK;
    }

    char *endptr;
    unsigned long friend_number = strtoul(argv[1], &endptr, 10);

    if (endptr == argv[1] || !tox_friend_exists(identity->tox, friend_number))
    {
        weechat_printf(identity->buffer,
                       "%sInvalid friend number.",
                       weechat_prefix("error"));
        return WEECHAT_RC_OK;
    }

    struct t_tox_weechat_chat *chat = tox_weechat_get_friend_chat(identity, friend_number);
    if (argc >= 3)
    {
        tox_send_message(identity->tox,
                         friend_number,
                         (uint8_t *)argv_eol[2],
                         strlen(argv_eol[2]));
        char *name = tox_weechat_get_self_name_nt(identity->tox);
        tox_weechat_chat_print_message(chat, name, argv_eol[2]);
        free(name);
    }

    return WEECHAT_RC_OK;
}

int
tox_weechat_cmd_myaddress(void *data, struct t_gui_buffer *buffer,
                          int argc, char **argv, char **argv_eol)
{
    struct t_tox_weechat_identity *identity = tox_weechat_identity_for_buffer(buffer);
    if (!identity)
    {
        weechat_printf(NULL,
                       "%s%s: command \"%s\" must be executed in a Tox buffer",
                       weechat_prefix("error"),
                       weechat_plugin->name,
                       argv[0]);
        return WEECHAT_RC_OK;
    }

    uint8_t address[TOX_FRIEND_ADDRESS_SIZE];
    tox_get_address(identity->tox, address);

    char *address_str = malloc(TOX_FRIEND_ADDRESS_SIZE * 2 + 1);
    tox_weechat_bin2hex(address, TOX_FRIEND_ADDRESS_SIZE, address_str);

    weechat_printf(identity->buffer,
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

    struct t_tox_weechat_identity *identity = tox_weechat_identity_for_buffer(buffer);
    if (!identity)
    {
        weechat_printf(NULL,
                       "%s%s: command \"%s\" must be executed on a Tox buffer",
                       weechat_prefix("error"),
                       weechat_plugin->name,
                       argv[0]);
        return WEECHAT_RC_OK;
    }

    char *name = argv_eol[1];

    int result = tox_set_name(identity->tox, (uint8_t *)name, strlen(name));
    if (result == -1)
    {
        weechat_printf(identity->buffer,
                       "%s%s",
                       weechat_prefix("error"),
                       "Could not change name.");
        return WEECHAT_RC_OK;
    }

    weechat_bar_item_update("input_prompt");

    weechat_printf(identity->buffer,
                   "%sYou are now known as %s",
                   weechat_prefix("network"),
                   name);

    for (struct t_tox_weechat_chat *chat = identity->chats;
         chat;
         chat = chat->next_chat)
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

    struct t_tox_weechat_identity *identity = tox_weechat_identity_for_buffer(buffer);
    if (!identity)
    {
        weechat_printf(NULL,
                       "%s%s: command \"%s\" must be executed in a Tox buffer",
                       weechat_prefix("error"),
                       weechat_plugin->name,
                       argv[0]);
        return WEECHAT_RC_OK;
    }

    TOX_USERSTATUS status = TOX_USERSTATUS_INVALID;
    if (weechat_strcasecmp(argv[1], "online") == 0)
        status = TOX_USERSTATUS_NONE;
    else if (weechat_strcasecmp(argv[1], "busy") == 0)
        status = TOX_USERSTATUS_BUSY;
    else if (weechat_strcasecmp(argv[1], "away") == 0)
        status = TOX_USERSTATUS_AWAY;

    if (status == TOX_USERSTATUS_INVALID)
        return WEECHAT_RC_ERROR;

    tox_set_user_status(identity->tox, status);
    weechat_bar_item_update("away");

    return WEECHAT_RC_OK;
}

int
tox_weechat_cmd_statusmsg(void *data, struct t_gui_buffer *buffer,
                          int argc, char **argv, char **argv_eol)
{
    struct t_tox_weechat_identity *identity = tox_weechat_identity_for_buffer(buffer);
    if (!identity)
    {
        weechat_printf(NULL,
                       "%s%s: command \"%s\" must be executed in a Tox buffer",
                       weechat_prefix("error"),
                       weechat_plugin->name,
                       argv[0]);
        return WEECHAT_RC_OK;
    }

    char *message = argc > 1 ? argv_eol[1] : " ";

    int result = tox_set_status_message(identity->tox,
                                        (uint8_t *)message,
                                        strlen(message));
    if (result == -1)
    {
        weechat_printf(identity->buffer,
                       "%s%s",
                       weechat_prefix("error"),
                       "Could not set status message.");
    }

    return WEECHAT_RC_OK;
}

int
tox_weechat_cmd_tox(void *data, struct t_gui_buffer *buffer,
                    int argc, char **argv, char **argv_eol)
{
    if (argc == 1 || (argc == 2 && weechat_strcasecmp(argv[1], "list") == 0))
    {
        weechat_printf(NULL,
                       "%sAll Tox identities:",
                       weechat_prefix("network"));
        for (struct t_tox_weechat_identity *identity = tox_weechat_identities;
             identity;
             identity = identity->next_identity)
        {
            weechat_printf(NULL,
                           "%s%s",
                           weechat_prefix("network"),
                           identity->name);
        }

        return WEECHAT_RC_OK;
    }

    else if (argc == 3 && (weechat_strcasecmp(argv[1], "add") == 0))
    {
        char *name = argv[2];

        if (tox_weechat_identity_name_search(name))
        {
            weechat_printf(NULL,
                           "%s%s: Identity \"%s\" already exists!",
                           weechat_prefix("error"),
                           weechat_plugin->name,
                           name);
            return WEECHAT_RC_OK;
        }

        struct t_tox_weechat_identity *identity = tox_weechat_identity_new(name);
        weechat_printf(NULL,
                       "%s%s: Identity \"%s\" created!",
                       weechat_prefix("network"),
                       weechat_plugin->name,
                       identity->name);

        return WEECHAT_RC_OK;
    }

    else if (argc == 3 && (weechat_strcasecmp(argv[1], "connect") == 0))
    {
        char *name = argv[2];

        struct t_tox_weechat_identity *identity = tox_weechat_identity_name_search(name);
        if (!identity)
        {
            weechat_printf(NULL,
                           "%s%s: Identity \"%s\" does not exist.",
                           weechat_prefix("error"),
                           weechat_plugin->name,
                           name);
        }
        else
        {
            tox_weechat_identity_connect(identity);
        }

        return WEECHAT_RC_OK;
    }

    return WEECHAT_RC_ERROR;
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

    weechat_hook_command("tox",
                         "manage Tox identities",
                         "list"
                         " || add <name>"
                         " || del <name>"
                         " || connect <name>",
                         "   list: list all Tox identity\n"
                         "    add: create a new Tox identity\n"
                         "    del: delete a Tox identity\n"
                         "connect: connect a Tox identity to the network\n",
                         NULL, tox_weechat_cmd_tox, NULL);
}
