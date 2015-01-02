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

#ifndef TOX_WEECHAT_MESSAGE_QUEUE_H
#define TOX_WEECHAT_MESSAGE_QUEUE_H

#include <time.h>

#include <tox/tox.h>

#include "twc-chat.h"

struct t_twc_profile;
struct t_twc_list_item;

struct t_twc_queued_message
{
    struct tm *time;
    char *message;
    enum TWC_MESSAGE_TYPE message_type;
};

void
twc_message_queue_add_friend_message(struct t_twc_profile *profile,
                                     int32_t friend_number,
                                     const char *message,
                                     enum TWC_MESSAGE_TYPE message_type);

void
twc_message_queue_flush_friend(struct t_twc_profile *profile,
                               int32_t friend_number);

void
twc_message_queue_free_message(struct t_twc_queued_message *message);

void
twc_message_queue_free_profile(struct t_twc_profile *profile);

#endif // TOX_WEECHAT_MESSAGE_QUEUE_H

