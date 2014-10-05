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
#include "twc-list.h"
#include "twc-profile.h"
#include "twc-utils.h"

#include "twc-message-queue.h"

/**
 * Get a message queue for a friend, or create one if it does not exist.
 */
struct t_twc_list *
twc_message_queue_get_or_create(struct t_twc_profile *profile,
                                int32_t friend_number)
{
    struct t_twc_list *message_queue = weechat_hashtable_get(profile->message_queues, &friend_number);
    if (!message_queue)
    {
        message_queue = twc_list_new();
        weechat_hashtable_set(profile->message_queues,
                              &friend_number,
                              message_queue);
    }

    return message_queue;
}

/**
 * Add a friend message to the message queue and tries to send it if the
 * friend is online. Handles splitting of messages. (TODO: actually split messages)
 */
void
twc_message_queue_add_friend_message(struct t_twc_profile *profile,
                                     int32_t friend_number,
                                     const char *message,
                                     enum TWC_MESSAGE_TYPE message_type)
{
    struct t_twc_queued_message *queued_message
        = malloc(sizeof(struct t_twc_queued_message));

    time_t rawtime = time(NULL);
    queued_message->time = malloc(sizeof(struct tm));
    memcpy(queued_message->time, gmtime(&rawtime), sizeof(struct tm));

    queued_message->message = strdup(message);
    queued_message->message_type = message_type;

    // create a queue if needed and add message
    struct t_twc_list *message_queue
        = twc_message_queue_get_or_create(profile, friend_number);
    twc_list_item_new_data_add(message_queue, queued_message);

    // flush if friend is online
    if (profile->tox
        && tox_get_friend_connection_status(profile->tox, friend_number) == 1)
        twc_message_queue_flush_friend(profile, friend_number);
}

/**
 * Try sending queued messages for a friend.
 */
void
twc_message_queue_flush_friend(struct t_twc_profile *profile,
                               int32_t friend_number)
{
    struct t_twc_list *message_queue
        = twc_message_queue_get_or_create(profile, friend_number);

    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach(message_queue, index, item)
    {
        struct t_twc_queued_message *queued_message = item->queued_message;

        // TODO: store and deal with message IDs
        uint32_t rc = 0;
        switch(queued_message->message_type)
        {
            case TWC_MESSAGE_TYPE_MESSAGE:
                rc = tox_send_message(profile->tox,
                                      friend_number,
                                      (uint8_t *)queued_message->message,
                                      strlen(queued_message->message));
                break;
            case TWC_MESSAGE_TYPE_ACTION:
                rc = tox_send_action(profile->tox,
                                     friend_number,
                                     (uint8_t *)queued_message->message,
                                     strlen(queued_message->message));
                break;
        }

        if (rc == 0)
        {
            // break if message send failed
            break;
        }
        else
        {
            // message was sent, free it
            twc_message_queue_free_message(queued_message);
            item->queued_message = NULL;
        }
    }

    // remove any now-empty items
    while (message_queue->head && !(message_queue->head->queued_message))
        twc_list_remove(message_queue->head);
}

/**
 * Free a queued message.
 */
void
twc_message_queue_free_message(struct t_twc_queued_message *message)
{
    free(message->time);
    free(message->message);
    free(message);
}

void
twc_message_queue_free_map_callback(void *data, struct t_hashtable *hashtable,
                                    const void *key, const void *value)
{
    struct t_twc_list *message_queue = ((struct t_twc_list *)value);

    struct t_twc_queued_message *message;
    while ((message = twc_list_pop(message_queue)))
        twc_message_queue_free_message(message);

    free(message_queue);
}

/**
 * Free the entire message queue for a profile.
 */
void
twc_message_queue_free_profile(struct t_twc_profile *profile)
{
    weechat_hashtable_map(profile->message_queues,
                          twc_message_queue_free_map_callback, NULL);
    weechat_hashtable_free(profile->message_queues);
}

