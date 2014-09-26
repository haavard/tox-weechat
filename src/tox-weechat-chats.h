/*
 * Copyright (c) 2015 Håvard Pettersson <haavard.pettersson@gmail.com>
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

#ifndef TOX_WEECHAT_CHATS_H
#define TOX_WEECHAT_CHATS_H

#include <stdint.h>

#include <tox/tox.h>

extern const char *tox_weechat_tag_unsent_message;
extern const char *tox_weechat_tag_sent_message;
extern const char *tox_weechat_tag_received_message;

struct t_tox_weechat_chat
{
    struct t_gui_buffer *buffer;

    int32_t friend_number;

    struct t_tox_weechat_identity *identity;

    struct t_tox_weechat_chat *next_chat;
    struct t_tox_weechat_chat *prev_chat;
};

struct t_tox_weechat_chat *
tox_weechat_get_friend_chat(struct t_tox_weechat_identity *identity,
                            int32_t friend_number);

struct t_tox_weechat_chat *
tox_weechat_get_existing_friend_chat(struct t_tox_weechat_identity *identity,
                                     int32_t friend_number);

struct t_tox_weechat_chat *
tox_weechat_get_chat_for_buffer(struct t_gui_buffer *target_buffer);

void tox_weechat_chat_print_message(struct t_tox_weechat_chat *chat,
                                    const char *sender,
                                    const char *message,
                                    const char *tags);

void tox_weechat_chat_print_action(struct t_tox_weechat_chat *chat,
                                   const char *sender,
                                   const char *message,
                                   const char *tags);

void
tox_weechat_chat_refresh(struct t_tox_weechat_chat *chat);

void
tox_weechat_chat_queue_refresh(struct t_tox_weechat_chat *chat);

#endif // TOX_WEECHAT_CHATS_H
