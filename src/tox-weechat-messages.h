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

#ifndef TOX_WEECHAT_MESSAGES_H
#define TOX_WEECHAT_MESSAGES_H

#include <tox/tox.h>

struct t_tox_weechat_identity;

struct t_tox_weechat_unsent_message_recipient
{
    uint8_t recipient_id[TOX_CLIENT_ID_SIZE];
    struct t_tox_weechat_identity *identity;

    struct t_tox_weechat_unsent_message *unsent_messages;
    struct t_tox_weechat_unsent_message *last_unsent_message;

    struct t_tox_weechat_unsent_message_recipient *next_recipient;
    struct t_tox_weechat_unsent_message_recipient *prev_recipient;
};

struct t_tox_weechat_unsent_message
{
    char *message;
    struct t_tox_weechat_unsent_message_recipient *recipient;

    struct t_tox_weechat_unsent_message *next_message;
    struct t_tox_weechat_unsent_message *prev_message;
};

uint32_t
tox_weechat_send_friend_message(struct t_tox_weechat_identity *identity,
                                int32_t friend_number,
                                const char *message);

void
tox_weechat_add_unsent_message(struct t_tox_weechat_identity *identity,
                               const uint8_t *recipient_id,
                               const char *message);

void
tox_weechat_unsent_messages_free(struct t_tox_weechat_identity *identity);

#endif // TOX_WEECHAT_MESSAGES_H
