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

#include <stdlib.h>
#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"
#include "tox-weechat-identities.h"
#include "tox-weechat-messages.h"
#include "tox-weechat-utils.h"

#include "tox-weechat-chats.h"

const char *tox_weechat_tag_unsent_message = "tox_unsent";
const char *tox_weechat_tag_sent_message = "tox_sent";
const char *tox_weechat_tag_received_message = "tox_received";

int tox_weechat_buffer_input_callback(void *data,
                                      struct t_gui_buffer *buffer,
                                      const char *input_data);

int tox_weechat_buffer_close_callback(void *data,
                                      struct t_gui_buffer *buffer);

void
tox_weechat_chat_add(struct t_tox_weechat_identity *identity,
                     struct t_tox_weechat_chat *chat)
{
    chat->identity = identity;

    chat->prev_chat = identity->last_chat;
    chat->next_chat = NULL;

    if (identity->chats == NULL)
        identity->chats = chat;
    else
        identity->last_chat->next_chat = chat;

    identity->last_chat = chat;
}

void
tox_weechat_chat_remove(struct t_tox_weechat_chat *chat)
{
    if (chat->prev_chat)
        chat->prev_chat->next_chat = chat->next_chat;
    if (chat->next_chat)
        chat->next_chat->prev_chat = chat->prev_chat;

    if (chat == chat->identity->chats)
        chat->identity->chats = chat->next_chat;
    if (chat == chat->identity->last_chat)
        chat->identity->last_chat = chat->prev_chat;

    free(chat);
}

void
tox_weechat_chat_refresh(struct t_tox_weechat_chat *chat)
{
    char *name = tox_weechat_get_name_nt(chat->identity->tox,
                                         chat->friend_number);
    char *status_message = tox_weechat_get_status_message_nt(chat->identity->tox,
                                                             chat->friend_number);

    weechat_buffer_set(chat->buffer, "short_name", name);
    weechat_buffer_set(chat->buffer, "title", status_message);

    free(name);
    free(status_message);
}

int
tox_weechat_chat_refresh_timer_callback(void *data, int remaining)
{
    struct t_tox_weechat_chat *chat = data;
    tox_weechat_chat_refresh(chat);

    return WEECHAT_RC_OK;
}

void
tox_weechat_chat_queue_refresh(struct t_tox_weechat_chat *chat)
{
    weechat_hook_timer(1, 0, 1,
                       tox_weechat_chat_refresh_timer_callback, chat);
}

struct t_tox_weechat_chat *
tox_weechat_friend_chat_new(struct t_tox_weechat_identity *identity,
                            int32_t friend_number)
{
    struct t_tox_weechat_chat *chat = malloc(sizeof(*chat));
    chat->friend_number = friend_number;
    chat->identity = identity;

    uint8_t client_id[TOX_CLIENT_ID_SIZE];
    tox_get_client_id(identity->tox, friend_number, client_id);

    // TODO: prepend identity name
    char buffer_name[TOX_CLIENT_ID_SIZE * 2 + 1];
    tox_weechat_bin2hex(client_id, TOX_CLIENT_ID_SIZE, buffer_name);

    chat->buffer = weechat_buffer_new(buffer_name,
                                      tox_weechat_buffer_input_callback, chat,
                                      tox_weechat_buffer_close_callback, chat);
    tox_weechat_chat_refresh(chat);
    tox_weechat_chat_add(identity, chat);

    return chat;
}

struct t_tox_weechat_chat *
tox_weechat_get_existing_friend_chat(struct t_tox_weechat_identity *identity,
                                     int32_t friend_number)
{
    for (struct t_tox_weechat_chat *chat = identity->chats;
         chat;
         chat = chat->next_chat)
    {
        if (chat->friend_number == friend_number)
            return chat;
    }

    return NULL;
}

struct t_tox_weechat_chat *
tox_weechat_get_friend_chat(struct t_tox_weechat_identity *identity,
                            int32_t friend_number)
{
    struct t_tox_weechat_chat *chat = tox_weechat_get_existing_friend_chat(identity, friend_number);

    if (chat)
        return chat;
    else
        return tox_weechat_friend_chat_new(identity, friend_number);
}

struct t_tox_weechat_chat *
tox_weechat_get_chat_for_buffer(struct t_gui_buffer *buffer)
{
    for (struct t_tox_weechat_identity *identity = tox_weechat_identities;
         identity;
         identity = identity->next_identity)
    {
        for (struct t_tox_weechat_chat *chat = identity->chats;
             chat;
             chat = chat->next_chat)
        {
            if (chat->buffer == buffer)
                return chat;
        }
    }

    return NULL;
}

void
tox_weechat_chat_print_message(struct t_tox_weechat_chat *chat,
                               const char *sender,
                               const char *message,
                               const char *tags)
{
    weechat_printf_tags(chat->buffer, tags, "%s\t%s", sender, message);
}

void
tox_weechat_chat_print_action(struct t_tox_weechat_chat *chat,
                              const char *sender,
                              const char *message,
                              const char *tags)
{
    weechat_printf_tags(chat->buffer, tags,
                        "%s%s %s",
                        weechat_prefix("action"),
                        sender, message);
}

int
tox_weechat_buffer_input_callback(void *data,
                                  struct t_gui_buffer *weechat_buffer,
                                  const char *input_data)
{
    struct t_tox_weechat_chat *chat = data;
    int rc = tox_weechat_send_friend_message(chat->identity,
                                             chat->friend_number,
                                             input_data);

    char *name = tox_weechat_get_self_name_nt(chat->identity->tox);
    tox_weechat_chat_print_message(chat, "", name, input_data);
    free(name);

    return WEECHAT_RC_OK;
}

int
tox_weechat_buffer_close_callback(void *data,
                                  struct t_gui_buffer *weechat_buffer)
{
    struct t_tox_weechat_chat *chat = data;
    tox_weechat_chat_remove(chat);

    return WEECHAT_RC_OK;
}

