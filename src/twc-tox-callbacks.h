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

#ifndef TOX_WEECHAT_TOX_CALLBACKS_H
#define TOX_WEECHAT_TOX_CALLBACKS_H

#include <tox/tox.h>

int
twc_do_timer_cb(void *data,
                int remaining_calls);

void
twc_friend_message_callback(Tox *tox,
                            int32_t friend_number,
                            const uint8_t *message,
                            uint16_t length,
                            void *data);

void
twc_friend_action_callback(Tox *tox,
                           int32_t friend_number,
                           const uint8_t *message,
                           uint16_t length,
                           void *data);

void
twc_connection_status_callback(Tox *tox,
                               int32_t friend_number,
                               uint8_t status,
                               void *data);

void
twc_name_change_callback(Tox *tox,
                         int32_t friend_number,
                         const uint8_t *name,
                         uint16_t length,
                         void *data);

void
twc_user_status_callback(Tox *tox,
                         int32_t friend_number,
                         uint8_t status,
                         void *data);

void
twc_status_message_callback(Tox *tox,
                            int32_t friend_number,
                            const uint8_t *message,
                            uint16_t length,
                            void *data);

void
twc_friend_request_callback(Tox *tox,
                            const uint8_t *public_key,
                            const uint8_t *message,
                            uint16_t length,
                            void *data);

void
twc_group_invite_callback(Tox *tox,
                          int32_t friend_number,
                          const uint8_t *invite_data,
                          uint16_t length,
                          void *data);

void
twc_group_message_callback(Tox *tox,
                           int32_t group_number,
                           int32_t peer_number,
                           const uint8_t *message,
                           uint16_t length,
                           void *data);

void
twc_group_action_callback(Tox *tox,
                          int32_t group_number,
                          int32_t peer_number,
                          const uint8_t *message,
                          uint16_t length,
                          void *data);

#endif // TOX_WEECHAT_TOX_CALLBACKS_H

