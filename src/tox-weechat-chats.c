#include <stdlib.h>
#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"
#include "tox-weechat-utils.h"

#include "tox-weechat-chats.h"

struct t_tox_chat *tox_weechat_chats_head = NULL;
struct t_tox_chat *tox_weechat_chats_tail = NULL;

int tox_weechat_buffer_input_callback(void *data,
                                      struct t_gui_buffer *buffer,
                                      const char *input_data);

int tox_weechat_buffer_close_callback(void *data,
                                      struct t_gui_buffer *buffer);

void
tox_weechat_chat_add(struct t_tox_chat *chat)
{
    if (tox_weechat_chats_head == NULL)
    {
        tox_weechat_chats_head = tox_weechat_chats_tail = chat;
        chat->prev = chat->next = NULL;
    }
    else
    {
        tox_weechat_chats_tail->next = chat;
        chat->prev = tox_weechat_chats_tail;
        chat->next = NULL;

        tox_weechat_chats_tail = chat;
    }
}

void
tox_weechat_chat_remove(struct t_tox_chat *chat)
{
    if (chat->prev)
        chat->prev->next = chat->next;
    if (chat->next)
        chat->next->prev = chat->prev;

    if (chat == tox_weechat_chats_head)
        tox_weechat_chats_head = chat->next;
    if (chat == tox_weechat_chats_tail)
        tox_weechat_chats_tail = chat->prev;

    free(chat);
}

void
tox_weechat_chat_refresh(struct t_tox_chat *chat)
{
    char *name = tox_weechat_get_name_nt(chat->friend_number);
    char *status_message = tox_weechat_get_status_message_nt(chat->friend_number);

    weechat_buffer_set(chat->buffer, "short_name", name);
    weechat_buffer_set(chat->buffer, "title", status_message);

    free(name);
    free(status_message);
}

struct t_tox_chat *
tox_weechat_friend_chat_new(int32_t friend_number)
{
    struct t_tox_chat *chat = malloc(sizeof(*chat));
    chat->friend_number = friend_number;

    uint8_t client_id[TOX_CLIENT_ID_SIZE];
    tox_get_client_id(tox, friend_number, client_id);

    char *buffer_name = tox_weechat_bin2hex(client_id, TOX_CLIENT_ID_SIZE);

    chat->buffer = weechat_buffer_new(buffer_name,
                                      tox_weechat_buffer_input_callback, chat,
                                      tox_weechat_buffer_close_callback, chat);
    tox_weechat_chat_refresh(chat);
    tox_weechat_chat_add(chat);

    free(buffer_name);

    return chat;
}

struct t_tox_chat *
tox_weechat_get_friend_chat(int32_t friend_number)
{
    for (struct t_tox_chat *chat = tox_weechat_chats_head;
         chat;
         chat = chat->next)
    {
        if (chat->friend_number == friend_number)
            return chat;
    }

    return tox_weechat_friend_chat_new(friend_number);
}

struct t_tox_chat *
tox_weechat_get_chat_for_buffer(struct t_gui_buffer *buffer)
{
    for (struct t_tox_chat *chat = tox_weechat_chats_head;
         chat;
         chat = chat->next)
    {
        if (chat->buffer == buffer)
            return chat;
    }

    return NULL;
}

void
tox_weechat_chat_print_message(struct t_tox_chat *chat,
                               const char *sender,
                               const char *message)
{
    weechat_printf(chat->buffer, "%s\t%s", sender, message);
}

void
tox_weechat_chat_print_action(struct t_tox_chat *chat,
                              const char *sender,
                              const char *message)
{
    weechat_printf(chat->buffer,
                   "%s%s %s",
                   weechat_prefix("action"),
                   sender, message);
}

int
tox_weechat_buffer_input_callback(void *data,
                                  struct t_gui_buffer *weechat_buffer,
                                  const char *input_data)
{
    struct t_tox_chat *chat = data;

    tox_send_message(tox,
                     chat->friend_number,
                     (uint8_t *)input_data,
                     strlen(input_data));

    char *name = tox_weechat_get_self_name_nt();

    tox_weechat_chat_print_message(chat, name, input_data);

    free(name);

    return WEECHAT_RC_OK;
}

int
tox_weechat_buffer_close_callback(void *data,
                                  struct t_gui_buffer *weechat_buffer)
{
    struct t_tox_chat *chat = data;
    tox_weechat_chat_remove(chat);

    return WEECHAT_RC_OK;
}

