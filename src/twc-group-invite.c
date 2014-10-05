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

#include "twc-group-invite.h"

/**
 * Add a new group invite to a profile.
 *
 * Returns the index of the invite on success, -1 on error.
 */
int
twc_group_chat_invite_add(struct t_twc_profile *profile,
                          int32_t friend_number,
                          uint8_t *data,
                          size_t size)
{
    // create a new invite object
    struct t_twc_group_chat_invite *invite
        = malloc(sizeof(struct t_twc_group_chat_invite));
    if (!invite)
        return -1;

    uint8_t *data_copy = malloc(size);
    memcpy(data_copy, data, size);

    invite->profile = profile;
    invite->friend_number = friend_number;
    invite->data = data_copy;
    invite->data_size = size;

    twc_list_item_new_data_add(profile->group_chat_invites, invite);

    return profile->group_chat_invites->count - 1;
}

/**
 * Accept a group chat invite. Remove and free the invite. Returns group chat
 * number. -1 on failure.
 */
int
twc_group_chat_invite_join(struct t_twc_group_chat_invite *invite)
{
    int rc = tox_join_groupchat(invite->profile->tox,
                                invite->friend_number,
                                invite->data,
                                invite->data_size);
    twc_group_chat_invite_remove(invite);

    return rc;
}

/**
 * Remove and free a group chat invite.
 */
void
twc_group_chat_invite_remove(struct t_twc_group_chat_invite *invite)
{
    twc_list_remove_with_data(invite->profile->group_chat_invites, invite);
    twc_group_chat_invite_free(invite);
}

/**
 * Get group chat invite with a given index.
 */
struct t_twc_group_chat_invite *
twc_group_chat_invite_with_index(struct t_twc_profile *profile,
                                 int64_t index)
{
    return twc_list_get(profile->group_chat_invites, index)->group_chat_invite;
}

/**
 * Free a group chat invite.
 */
void
twc_group_chat_invite_free(struct t_twc_group_chat_invite *invite)
{
    free(invite->data);
    free(invite);
}

/**
 * Free a list of group chat invites.
 */
void
twc_group_chat_invite_free_list(struct t_twc_list *list)
{
    struct t_twc_group_chat_invite *invite;
    while ((invite = twc_list_pop(list)))
        twc_group_chat_invite_free(invite);

    free(list);
}

