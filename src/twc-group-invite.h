/*
 * Copyright (c) 2017 Håvard Pettersson <mail@haavard.me>
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

#ifndef TOX_WEECHAT_GROUP_INVITE_H
#define TOX_WEECHAT_GROUP_INVITE_H

#include <stdlib.h>

#include <tox/tox.h>

struct t_twc_list;

/**
 * Represents a group chat invite.
 */
struct t_twc_group_chat_invite
{
    struct t_twc_profile *profile;

    uint32_t friend_number;
    uint8_t group_chat_type;
    uint8_t *data;
    size_t data_size;
    uint32_t autojoin_delay;
};

int
twc_group_chat_invite_add(struct t_twc_profile *profile, int32_t friend_number,
                          uint8_t group_chat_type, uint8_t *data, size_t size);

int
twc_group_chat_invite_join(struct t_twc_group_chat_invite *invite);

void
twc_group_chat_invite_remove(struct t_twc_group_chat_invite *invite);

struct t_twc_group_chat_invite *
twc_group_chat_invite_with_index(struct t_twc_profile *profile, size_t index);

void
twc_group_chat_invite_free(struct t_twc_group_chat_invite *invite);

void
twc_group_chat_invite_free_list(struct t_twc_list *list);

#endif /* TOX_WEECHAT_GROUP_INVITE_H */
