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

#include <string.h>

#include <tox/tox.h>
#include <weechat/weechat-plugin.h>

#ifdef TOXAV_ENABLED
#include <tox/toxav.h>
#endif /* TOXAV_ENABLED */

#include "twc-list.h"
#include "twc-profile.h"
#include "twc-utils.h"
#include "twc.h"

#include "twc-group-invite.h"

/**
 * Add a new group invite to a profile.
 *
 * Returns the index of the invite on success, -1 on error.
 */
int
twc_group_chat_invite_add(struct t_twc_profile *profile, int32_t friend_number,
                          uint8_t group_chat_type, uint8_t *data, size_t size)
{
    /* create a new invite object */
    struct t_twc_group_chat_invite *invite =
        malloc(sizeof(struct t_twc_group_chat_invite));
    if (!invite)
        return -1;

    uint8_t *data_copy = malloc(size);
    memcpy(data_copy, data, size);

    invite->profile = profile;
    invite->friend_number = friend_number;
    invite->group_chat_type = group_chat_type;
    invite->data = data_copy;
    invite->data_size = size;

    if (TWC_PROFILE_OPTION_BOOLEAN(profile, TWC_PROFILE_OPTION_AUTOJOIN))
        invite->autojoin_delay = TWC_PROFILE_OPTION_INTEGER(profile, TWC_PROFILE_OPTION_AUTOJOIN_DELAY);
    else
        invite->autojoin_delay = 0;

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
    int rc;
    TOX_ERR_CONFERENCE_JOIN err = TOX_ERR_CONFERENCE_JOIN_OK;
    switch (invite->group_chat_type)
    {
        case TOX_CONFERENCE_TYPE_TEXT:
            rc =
                tox_conference_join(invite->profile->tox, invite->friend_number,
                                    invite->data, invite->data_size, &err);
            break;
#ifdef TOXAV_ENABLED
        case TOX_CONFERENCE_TYPE_AV:
            rc = toxav_join_av_groupchat(invite->profile->tox,
                                         invite->friend_number, invite->data,
                                         invite->data_size, NULL, NULL);
            break;
#endif
        default:
            rc = -1;
            break;
    }

    twc_group_chat_invite_remove(invite);

    if (err != TOX_ERR_CONFERENCE_JOIN_OK)
        return -1;
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
twc_group_chat_invite_with_index(struct t_twc_profile *profile, size_t index)
{
    struct t_twc_list_item *item =
        twc_list_get(profile->group_chat_invites, index);
    if (item)
        return item->group_chat_invite;
    else
        return NULL;
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
