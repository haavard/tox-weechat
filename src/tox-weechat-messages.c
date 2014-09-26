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

/**
 * Return an existing unsent message recipient object or NULL.
 */
struct t_tox_weechat_unsent_message_recipient *
tox_weechat_unsent_message_recipient_with_id(struct t_tox_weechat_identity *identity,
                                             const uint8_t *id)
{
    struct t_tox_weechat_unsent_message_recipient *recipient;
    for (recipient = identity->unsent_message_recipients;
         recipient;
         recipient = recipient->next_recipient)
    {
        if (memcmp(recipient->recipient_id, id, TOX_CLIENT_ID_SIZE) == 0)
            return recipient;
    }

    return NULL;
}

/**
 * Create and return a new unsent message recipient object.
 */
struct t_tox_weechat_unsent_message_recipient *
tox_weechat_unsent_message_recipient_new(struct t_tox_weechat_identity *identity,
                                         const uint8_t *id)
{
    struct t_tox_weechat_unsent_message_recipient *recipient = malloc(sizeof(*recipient));
    if (!recipient)
        return NULL;

    memcpy(recipient->recipient_id, id, TOX_CLIENT_ID_SIZE);
    recipient->identity = identity;
    recipient->unsent_messages = recipient->last_unsent_message = NULL;

    recipient->prev_recipient = identity->last_unsent_message_recipient;
    recipient->next_recipient = NULL;

    if (identity->unsent_message_recipients == NULL)
        identity->unsent_message_recipients = recipient;
    else
        identity->last_unsent_message_recipient->next_recipient = recipient;

    identity->last_unsent_message_recipient = recipient;

    return recipient;
}

/**
 * Add a new message to the unsent messages queue.
 */
void
tox_weechat_add_unsent_message(struct t_tox_weechat_identity *identity,
                               const uint8_t *recipient_id,
                               const char *message)
{
    struct t_tox_weechat_unsent_message_recipient *recipient
        = tox_weechat_unsent_message_recipient_with_id(identity, recipient_id);
    if (!recipient)
        recipient = tox_weechat_unsent_message_recipient_new(identity, recipient_id);
    if (!recipient)
        return;

    struct t_tox_weechat_unsent_message *unsent_message = malloc(sizeof(*unsent_message));
    if (!message)
        return;

    unsent_message->message = strdup(message);
    unsent_message->recipient = recipient;

    unsent_message->prev_message = recipient->last_unsent_message;
    unsent_message->next_message = NULL;

    if (recipient->unsent_messages == NULL)
        recipient->unsent_messages = unsent_message;
    else
        recipient->last_unsent_message->next_message = unsent_message;

    recipient->last_unsent_message = unsent_message;
}

void
tox_weechat_remove_unsent_message(struct t_tox_weechat_unsent_message *message)
{
    struct t_tox_weechat_unsent_message_recipient *recipient = message->recipient;
    if (message == recipient->last_unsent_message)
        recipient->last_unsent_message = message->prev_message;

    if (message->prev_message)
        message->prev_message->next_message = message->next_message;
    else
        recipient->unsent_messages = message->next_message;

    if (message->next_message)
        message->next_message->prev_message = message->prev_message;

    free(message->message);
    free(message);
}

/**
 * Sends a message to a friend. Does message splitting and queuing.
 */
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

void
tox_weechat_unsent_messages_free(struct t_tox_weechat_identity *identity)
{
    struct t_tox_weechat_unsent_message_recipient *recipient;
    for (recipient = identity->unsent_message_recipients;
         recipient;
         recipient = recipient->next_recipient)
    {
        while (recipient->unsent_messages)
            tox_weechat_remove_unsent_message(recipient->unsent_messages);
    }
}

void
tox_weechat_attempt_message_flush(struct t_tox_weechat_identity *identity,
                                  int32_t friend_number)
{

}
