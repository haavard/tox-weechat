#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"
#include "tox-weechat-utils.h"
#include "tox-weechat-chats.h"
#include "tox-weechat-friend-requests.h"

#include "tox-weechat-commands.h"

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
                       "%s[ID] Name [client ID]",
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

    if (argc == 3 && (weechat_strcasecmp(argv[1], "add") == 0))
    {
        weechat_printf(tox_main_buffer, "TODO: friend add");

        return WEECHAT_RC_OK;
    }

    // /friend accept
    if (argc == 3 &&
            (weechat_strcasecmp(argv[1], "accept") == 0
             || weechat_strcasecmp(argv[1], "decline")))
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
            unsigned int num = atoi(argv[1]);
            if (num == 0 || (request = tox_weechat_friend_request_with_num(num)) == NULL)
            {
                weechat_printf(tox_main_buffer,
                               "%sInvalid friend request ID.",
                               weechat_prefix("error"));
                return WEECHAT_RC_OK;
            }

            if (accept)
                tox_weechat_accept_friend_request(request);
            else
                tox_weechat_decline_friend_request(request);

            char *hex_id = tox_weechat_bin2hex(request->address, TOX_CLIENT_ID_SIZE);
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
tox_weechat_cmd_me(void *data, struct t_gui_buffer *buffer,
                   int argc, char **argv, char **argv_eol)
{
    struct t_tox_chat *chat = tox_weechat_get_chat_for_buffer(buffer);

    tox_send_message(tox,
                     chat->friend_number,
                     (uint8_t *)argv_eol[1],
                     strlen(argv_eol[1]));

    char *name = tox_weechat_get_self_name_nt();
    tox_weechat_chat_print_action(chat, name, argv_eol[1]);

    free(name);

    return WEECHAT_RC_OK;
}

int
tox_weechat_cmd_name(void *data, struct t_gui_buffer *buffer,
                     int argc, char **argv, char **argv_eol)
{
    char *message = argv_eol[1];

    int result = tox_set_name(tox, (uint8_t *)message, strlen(message));
    if (result == -1)
    {
        weechat_printf(tox_main_buffer,
                       "%s%s",
                       weechat_prefix("error"),
                       "Could not change name.");
        return WEECHAT_RC_ERROR;
    }

    weechat_bar_item_update("input_prompt");

    return WEECHAT_RC_OK;
}

void
tox_weechat_commands_init()
{
    weechat_hook_command("friend",
                         "manage friends",
                         "list"
                         " || add <address>"
                         " || remove <name>|<address>|<id>"
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

    weechat_hook_command("myaddress",
                         "get your Tox address to give to friends",
                         "", "",
                         NULL, tox_weechat_cmd_myaddress, NULL);

    weechat_hook_command("name",
                         "change your Tox name",
                         "<name>",
                         "name: your new name",
                         NULL, tox_weechat_cmd_name, NULL);
}
