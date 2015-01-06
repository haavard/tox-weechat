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

#ifndef TOX_WEECHAT_FRIEND_REQUEST_H
#define TOX_WEECHAT_FRIEND_REQUEST_H

#include <tox/tox.h>

struct t_twc_list;

/**
 * Represents a friend request with a Tox ID and a message.
 */
struct t_twc_friend_request
{
    struct t_twc_profile *profile;

    uint8_t tox_id[TOX_CLIENT_ID_SIZE];
    char *message;
};

int
twc_friend_request_add(struct t_twc_profile *profile,
                       const uint8_t *client_id,
                       const char *message);

void
twc_friend_request_accept(struct t_twc_friend_request *request);

void
twc_friend_request_remove(struct t_twc_friend_request *request);

struct t_twc_friend_request *
twc_friend_request_with_index(struct t_twc_profile *profile,
                              int64_t index);

void
twc_friend_request_free(struct t_twc_friend_request *request);

void
twc_friend_request_free_list(struct t_twc_list *list);

#endif // TOX_WEECHAT_FRIEND_REQUEST_H

