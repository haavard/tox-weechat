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

#include <tox/tox.h>

#include "tox-weechat-identities.h"
#include "tox-weechat-utils.h"

#include "tox-weechat-messages.h"

void
tox_weechat_add_unsent_message(struct t_tox_weechat_identity *identity,
                               const uint8_t *recipient_id,
                               const char *message)
{
    struct t_tox_weechat_unsent_message *unsent_message = malloc(sizeof(*unsent_message));
    if (!message)
        return;

    memcpy(unsent_message->recipient_id, recipient_id, TOX_CLIENT_ID_SIZE);
    unsent_message->message = strdup(message);

    unsent_message->prev_message = identity->last_unsent_message;
    unsent_message->next_message = NULL;

    if (identity->unsent_messages == NULL)
        identity->unsent_messages = unsent_message;
    else
        identity->last_unsent_message->next_message = unsent_message;

    identity->last_unsent_message = unsent_message;
}

uint32_t
tox_weechat_send_friend_message(struct t_tox_weechat_identity *identity,
                                int32_t friend_number,
                                const char *message)
{
    // TODO: split message
    uint32_t rc = tox_send_message(identity->tox,
                                   friend_number,
                                   (uint8_t *)message,
                                   strlen(message));

    uint8_t recipient_id[TOX_CLIENT_ID_SIZE];
    tox_get_client_id(identity->tox, friend_number, recipient_id);

    if (rc == 0)
        tox_weechat_add_unsent_message(identity, recipient_id, message);

    return rc;
}

