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
#include "twc-profile.h"
#include "twc-chat.h"
#include "twc-friend-request.h"
#include "twc-group-invite.h"
#include "twc-message-queue.h"
#include "twc-utils.h"

#include "twc-tox-callbacks.h"

int
twc_do_timer_cb(void *data,
                int remaining_calls)
{
    struct t_twc_profile *profile = data;

    tox_do(profile->tox);
    struct t_hook *hook = weechat_hook_timer(tox_do_interval(profile->tox), 0, 1,
                                             twc_do_timer_cb, profile);
    profile->tox_do_timer = hook;

    // check connection status
    int connected = tox_isconnected(profile->tox);
    twc_profile_set_online_status(profile, connected);

    return WEECHAT_RC_OK;
}

void
twc_handle_friend_message(Tox *tox,
                          int32_t friend_number,
                          const uint8_t *message,
                          uint16_t length,
                          void *data,
                          enum TWC_MESSAGE_TYPE message_type)

{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat = twc_chat_search_friend(profile,
                                                     friend_number,
                                                     true);

    char *name = twc_get_name_nt(profile->tox, friend_number);
    char *message_nt = twc_null_terminate(message, length);

    twc_chat_print_message(chat, "", name,
                           message_nt, message_type);

    free(name);
    free(message_nt);
}

void
twc_friend_message_callback(Tox *tox,
                            int32_t friend_number,
                            const uint8_t *message,
                            uint16_t length,
                            void *data)
{
    twc_handle_friend_message(tox,
                              friend_number,
                              message,
                              length,
                              data,
                              TWC_MESSAGE_TYPE_MESSAGE);
}

void
twc_friend_action_callback(Tox *tox,
                           int32_t friend_number,
                           const uint8_t *message,
                           uint16_t length,
                           void *data)
{
    twc_handle_friend_message(tox,
                              friend_number,
                              message,
                              length,
                              data,
                              TWC_MESSAGE_TYPE_ACTION);
}

void
twc_connection_status_callback(Tox *tox,
                               int32_t friend_number,
                               uint8_t status,
                               void *data)
{
    struct t_twc_profile *profile = data;
    char *name = twc_get_name_nt(profile->tox, friend_number);

    // TODO: print in friend's buffer if it exists
    if (status == 0)
    {
        weechat_printf(profile->buffer,
                       "%s%s just went offline.",
                       weechat_prefix("network"),
                       name);
    }
    else if (status == 1)
    {
        weechat_printf(profile->buffer,
                       "%s%s just came online.",
                       weechat_prefix("network"),
                       name);
        twc_message_queue_flush_friend(profile, friend_number);
    }
    free(name);
}

void
twc_name_change_callback(Tox *tox,
                         int32_t friend_number,
                         const uint8_t *name,
                         uint16_t length,
                         void *data)
{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat = twc_chat_search_friend(profile,
                                                     friend_number,
                                                     false);

    char *old_name = twc_get_name_nt(profile->tox, friend_number);
    char *new_name = twc_null_terminate(name, length);

    if (strcmp(old_name, new_name) != 0)
    {
        if (chat)
        {
            twc_chat_queue_refresh(chat);

            weechat_printf(chat->buffer,
                           "%s%s is now known as %s",
                           weechat_prefix("network"),
                           old_name, new_name);
        }

        weechat_printf(profile->buffer,
                       "%s%s is now known as %s",
                       weechat_prefix("network"),
                       old_name, new_name);
    }

    free(old_name);
    free(new_name);
}

void
twc_user_status_callback(Tox *tox,
                         int32_t friend_number,
                         uint8_t status,
                         void *data)
{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat = twc_chat_search_friend(profile,
                                                     friend_number,
                                                     false);
    if (chat)
        twc_chat_queue_refresh(chat);
}

void
twc_status_message_callback(Tox *tox,
                            int32_t friend_number,
                            const uint8_t *message,
                            uint16_t length,
                            void *data)
{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat = twc_chat_search_friend(profile,
                                                     friend_number,
                                                     false);
    if (chat)
        twc_chat_queue_refresh(chat);
}

void
twc_friend_request_callback(Tox *tox,
                            const uint8_t *public_key,
                            const uint8_t *message,
                            uint16_t length,
                            void *data)
{
    struct t_twc_profile *profile = data;

    char *message_nt = twc_null_terminate(message, length);
    int rc = twc_friend_request_add(profile, public_key, message_nt);

    if (rc == -1)
    {
        weechat_printf(profile->buffer,
                       "%sReceived a friend request, but your friend request list is full!",
                       weechat_prefix("warning"));
    }
    else
    {
        char hex_address[TOX_CLIENT_ID_SIZE * 2 + 1];
        twc_bin2hex(public_key, TOX_CLIENT_ID_SIZE, hex_address);

        weechat_printf(profile->buffer,
                       "%sReceived a friend request from %s with message \"%s\"; "
                       "accept it with \"/friend accept %d\"",
                       weechat_prefix("network"),
                       hex_address,
                       message_nt,
                       rc);

        if (rc == -2)
        {
            weechat_printf(profile->buffer,
                           "%sFailed to save friend request, try manually "
                           "accepting with /friend add",
                           weechat_prefix("error"));
        }
    }

    free(message_nt);
}

void
twc_group_invite_callback(Tox *tox,
                          int32_t friend_number,
                          const uint8_t *invite_data,
                          uint16_t length,
                          void *data)
{
    struct t_twc_profile *profile = data;

    int64_t rc = twc_group_chat_invite_add(profile, friend_number,
                                           (uint8_t *)invite_data, length);

    char *friend_name = twc_get_name_nt(profile->tox, friend_number);
    weechat_printf(profile->buffer,
                   "%sReceived a group chat invite from %s; "
                   "join with \"/group join %d\"",
                   weechat_prefix("network"),
                   friend_name, rc);

    free(friend_name);
}

void
twc_handle_group_message(Tox *tox,
                         int32_t group_number,
                         int32_t peer_number,
                         const uint8_t *message,
                         uint16_t length,
                         void *data,
                         enum TWC_MESSAGE_TYPE message_type)
{
    struct t_twc_profile *profile = data;

    struct t_twc_chat *chat = twc_chat_search_group(profile,
                                                    group_number,
                                                    true);

    char *name = twc_get_peer_name_nt(profile->tox, group_number, peer_number);
    char *message_nt = twc_null_terminate(message, length);

    twc_chat_print_message(chat, "", name,
                           message_nt, message_type);

    free(name);
    free(message_nt);
}

void
twc_group_message_callback(Tox *tox,
                           int32_t group_number,
                           int32_t peer_number,
                           const uint8_t *message,
                           uint16_t length,
                           void *data)
{
    twc_handle_group_message(tox,
                             group_number,
                             peer_number,
                             message,
                             length,
                             data,
                             TWC_MESSAGE_TYPE_MESSAGE);
}

void
twc_group_action_callback(Tox *tox,
                          int32_t group_number,
                          int32_t peer_number,
                          const uint8_t *message,
                          uint16_t length,
                          void *data)
{
    twc_handle_group_message(tox,
                             group_number,
                             peer_number,
                             message,
                             length,
                             data,
                             TWC_MESSAGE_TYPE_ACTION);
}

