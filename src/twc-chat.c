/*
 * Copyright (c) 2015 Håvard Pettersson <mail@haavard.me>
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

#include <stdio.h>
#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "twc.h"
#include "twc-list.h"
#include "twc-profile.h"
#include "twc-message-queue.h"
#include "twc-utils.h"

#include "twc-chat.h"

const char *twc_tag_unsent_message = "tox_unsent";
const char *twc_tag_sent_message = "tox_sent";
const char *twc_tag_received_message = "tox_received";

int
twc_chat_buffer_input_callback(void *data,
                               struct t_gui_buffer *weechat_buffer,
                               const char *input_data);
int
twc_chat_buffer_close_callback(void *data,
                               struct t_gui_buffer *weechat_buffer);

/**
 * Hash a Tox ID for the nicklist hashtable in group chat objects.
 */
unsigned long long
twc_tox_id_hash_callback(struct t_hashtable *hashtable, const void *key)
{
    return twc_hash_tox_id(key);
}

/**
 * Compare two Tox IDs for the nicklist hashtable in group chat objects.
 */
int
twc_tox_id_compare_callback(struct t_hashtable *hashtable,
                            const void *id1, const void *id2)
{
    return memcmp(id1, id2, TOX_CLIENT_ID_SIZE);
}

/**
 * Create a new chat.
 */
struct t_twc_chat *
twc_chat_new(struct t_twc_profile *profile, const char *name)
{
    struct t_twc_chat *chat = malloc(sizeof(struct t_twc_chat));
    if (!chat)
        return NULL;

    chat->profile = profile;
    chat->friend_number = chat->group_number = -1;
    chat->nicks = NULL;

    size_t full_name_size = strlen(profile->name) + 1 + strlen(name) + 1;
    char *full_name = malloc(full_name_size);
    snprintf(full_name, full_name_size, "%s/%s", profile->name, name);
    chat->buffer = weechat_buffer_new(full_name,
                                      twc_chat_buffer_input_callback, chat,
                                      twc_chat_buffer_close_callback, chat);
    free(full_name);

    if (!(chat->buffer))
    {
        free(chat);
        return NULL;
    }

    twc_chat_queue_refresh(chat);
    twc_list_item_new_data_add(profile->chats, chat);

    return chat;
}

/**
 * Create a new friend chat.
 */
struct t_twc_chat *
twc_chat_new_friend(struct t_twc_profile *profile, int32_t friend_number)
{
    uint8_t client_id[TOX_CLIENT_ID_SIZE];
    tox_get_client_id(profile->tox, friend_number, client_id);

    char buffer_name[TOX_CLIENT_ID_SIZE * 2 + 1];
    twc_bin2hex(client_id, TOX_CLIENT_ID_SIZE, buffer_name);

    struct t_twc_chat *chat = twc_chat_new(profile, buffer_name);
    if (chat)
        chat->friend_number = friend_number;

    return chat;
}

/**
 * Create a new group chat.
 */
struct t_twc_chat *
twc_chat_new_group(struct t_twc_profile *profile, int32_t group_number)
{
    char buffer_name[32];
    sprintf(buffer_name, "group_chat_%d", group_number);

    struct t_twc_chat *chat = twc_chat_new(profile, buffer_name);
    if (chat)
    {
        chat->group_number = group_number;

        chat->nicklist_group = weechat_nicklist_add_group(chat->buffer, NULL,
                                                          NULL, NULL, true);
        chat->nicks = weechat_hashtable_new(32,
                                            WEECHAT_HASHTABLE_BUFFER,
                                            WEECHAT_HASHTABLE_POINTER,
                                            twc_tox_id_hash_callback,
                                            twc_tox_id_compare_callback);

        weechat_buffer_set(chat->buffer, "nicklist", "1");
    }

    return chat;
}

/**
 * Refresh a chat. Updates buffer short_name and title.
 */
void
twc_chat_refresh(struct t_twc_chat *chat)
{
    char *name = NULL;
    char *title = NULL;
    if (chat->friend_number >= 0)
    {
        name = twc_get_name_nt(chat->profile->tox,
                               chat->friend_number);
        title = twc_get_status_message_nt(chat->profile->tox,
                                          chat->friend_number);
    }
    else if (chat->group_number >= 0)
    {
        char group_name[TOX_MAX_NAME_LENGTH + 1] = {0};
        int len = tox_group_get_title(chat->profile->tox, chat->group_number,
                                      (uint8_t *)group_name,
                                      TOX_MAX_NAME_LENGTH);
        if (len <= 0)
            sprintf(group_name, "Group Chat %d", chat->group_number);

        name = title = strdup((char *)group_name);
    }

    weechat_buffer_set(chat->buffer, "short_name", name);
    weechat_buffer_set(chat->buffer, "title", title);

    if (name)
        free(name);
    if (title && title != name)
        free(title);
}

/**
 * Callback for twc_chat_queue_refresh. Simply calls twc_chat_refresh.
 */
int
twc_chat_refresh_timer_callback(void *data, int remaining)
{
    twc_chat_refresh(data);

    return WEECHAT_RC_OK;
}

/**
 * Queue a refresh of the buffer in 1ms (i.e. the next event loop tick). Done
 * this way to allow data to update before refreshing interface.
 */
void
twc_chat_queue_refresh(struct t_twc_chat *chat)
{
    weechat_hook_timer(1, 0, 1,
                       twc_chat_refresh_timer_callback, chat);
}

/**
 * Find an existing chat object for a friend, and if not found, optionally
 * create a new one.
 */
struct t_twc_chat *
twc_chat_search_friend(struct t_twc_profile *profile,
                       int32_t friend_number, bool create_new)
{
    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach(profile->chats, index, item)
    {
        if (item->chat->friend_number == friend_number)
            return item->chat;
    }

    if (create_new)
        return twc_chat_new_friend(profile, friend_number);

    return NULL;
}

/**
 * Find an existing chat object for a friend, and if not found, optionally
 * create a new one.
 */
struct t_twc_chat *
twc_chat_search_group(struct t_twc_profile *profile,
                      int32_t group_number, bool create_new)
{
    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach(profile->chats, index, item)
    {
        if (item->chat->group_number == group_number)
            return item->chat;
    }

    if (create_new)
        return twc_chat_new_group(profile, group_number);

    return NULL;
}

/**
 * Find the chat object associated with a buffer, if it exists.
 */
struct t_twc_chat *
twc_chat_search_buffer(struct t_gui_buffer *buffer)
{
    size_t profile_index;
    struct t_twc_list_item *profile_item;
    twc_list_foreach(twc_profiles, profile_index, profile_item)
    {
        size_t chat_index;
        struct t_twc_list_item *chat_item;
        twc_list_foreach(profile_item->profile->chats, chat_index, chat_item)
        {
            if (chat_item->chat->buffer == buffer)
            {
                return chat_item->chat;
            }
        }
    }
    return NULL;
}

/**
 * Print a chat message to a chat's buffer.
 */
void
twc_chat_print_message(struct t_twc_chat *chat,
                       const char *tags,
                       const char *sender,
                       const char *message,
                       enum TWC_MESSAGE_TYPE message_type)
{
    switch (message_type)
    {
        case TWC_MESSAGE_TYPE_MESSAGE:
            weechat_printf_tags(chat->buffer, tags,
                                "%s\t%s",
                                sender, message);
            break;
        case TWC_MESSAGE_TYPE_ACTION:
            weechat_printf_tags(chat->buffer, tags,
                                "%s%s %s",
                                weechat_prefix("action"),
                                sender, message);
            break;
    }
}

/**
 * Send a message to the recipient(s) of a chat.
 */
void
twc_chat_send_message(struct t_twc_chat *chat, const char *message,
                      enum TWC_MESSAGE_TYPE message_type)
{
    if (chat->friend_number >= 0)
    {
        twc_message_queue_add_friend_message(chat->profile,
                                             chat->friend_number,
                                             message, message_type);
        char *name = twc_get_self_name_nt(chat->profile->tox);
        twc_chat_print_message(chat, "", name, message, message_type);
        free(name);
    }
    else if (chat->group_number >= 0)
    {
        if (message_type == TWC_MESSAGE_TYPE_MESSAGE)
            tox_group_message_send(chat->profile->tox, chat->group_number,
                                   (uint8_t *)message, strlen(message));
        else if (message_type == TWC_MESSAGE_TYPE_ACTION)
            tox_group_action_send(chat->profile->tox, chat->group_number,
                                  (uint8_t *)message, strlen(message));
    }
}

/**
 * Callback for a buffer receiving user input.
 */
int
twc_chat_buffer_input_callback(void *data, struct t_gui_buffer *weechat_buffer,
                               const char *input_data)
{
    struct t_twc_chat *chat = data;
    twc_chat_send_message(chat, input_data, TWC_MESSAGE_TYPE_MESSAGE);

    return WEECHAT_RC_OK;
}

/**
 * Callback for a buffer being closed.
 */
int
twc_chat_buffer_close_callback(void *data, struct t_gui_buffer *weechat_buffer)
{
    struct t_twc_chat *chat = data;

    if (chat->profile->tox && chat->group_number >= 0)
    {
        int rc = tox_del_groupchat(chat->profile->tox, chat->group_number);
        if (rc != 0)
        {
            weechat_printf(chat->profile->buffer,
                           "%swarning: failed to leave group chat",
                           weechat_prefix("error"));
        }
    }

    twc_list_remove_with_data(chat->profile->chats, chat);
    twc_chat_free(chat);

    return WEECHAT_RC_OK;
}

/**
 * Free a chat object.
 */
void
twc_chat_free(struct t_twc_chat *chat)
{
    if (chat->nicks)
        weechat_hashtable_free(chat->nicks);
    free(chat);
}

/**
 * Free all chats connected to a profile.
 */
void
twc_chat_free_list(struct t_twc_list *list)
{
    struct t_twc_chat *chat;
    while ((chat = twc_list_pop(list)))
    {
        weechat_buffer_set_pointer(chat->buffer, "close_callback", NULL);
        weechat_buffer_close(chat->buffer);
        twc_chat_free(chat);
    }

    free(list);
}

