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

#include <string.h>
#include <stdio.h>

#include <weechat/weechat-plugin.h>

#include "twc.h"
#include "twc-list.h"
#include "twc-profile.h"
#include "twc-utils.h"

#include "twc-completion.h"

enum
{
    TWC_ALL_PROFILES,
    TWC_LOADED_PROFILES,
    TWC_UNLOADED_PROFILES,
};

enum
{
    TWC_COMPLETE_FRIEND_NAME = 2 << 0,
    TWC_COMPLETE_FRIEND_ID = 1 << 1,
};

/**
 * Complete a friends name and/or Tox ID.
 */
int
twc_completion_friend(void *data,
                      const char *completion_item,
                      struct t_gui_buffer *buffer,
                      struct t_gui_completion *completion)
{
    int flags = (int)(intptr_t)data;
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);

    if (!profile)
        return WEECHAT_RC_OK;

    uint32_t friend_count = tox_count_friendlist(profile->tox);
    int32_t *friend_numbers = malloc(sizeof(int32_t) * friend_count);
    tox_get_friendlist(profile->tox, friend_numbers, friend_count);

    for (uint32_t i = 0; i < friend_count; ++i)
    {
        if (flags & TWC_COMPLETE_FRIEND_ID)
        {
            uint8_t tox_id[TOX_CLIENT_ID_SIZE];
            char hex_id[TOX_CLIENT_ID_SIZE * 2 + 1];

            tox_get_client_id(profile->tox, friend_numbers[i], tox_id);
            twc_bin2hex(tox_id, TOX_CLIENT_ID_SIZE, hex_id);

            weechat_hook_completion_list_add(completion, hex_id, 0,
                                             WEECHAT_LIST_POS_SORT);
        }

        if (flags & TWC_COMPLETE_FRIEND_NAME)
        {
            char *name = twc_get_name_nt(profile->tox, friend_numbers[i]);

            // add quotes if needed
            if (strchr(name, ' '))
            {
                size_t length = strlen(name) + 3;
                char *quoted_name = malloc(length);
                snprintf(quoted_name, length, "\"%s\"", name);

                free(name);
                name = quoted_name;
            }

            weechat_hook_completion_list_add(completion, name, 0,
                                             WEECHAT_LIST_POS_SORT);

            free(name);
        }
    }

    return WEECHAT_RC_OK;
}

/**
 * Complete a profile name, possibly filtering by loaded/unloaded profiles.
 */
int
twc_completion_profile(void *data,
                       const char *completion_item,
                       struct t_gui_buffer *buffer,
                       struct t_gui_completion *completion)
{
    int flag = (int)(intptr_t)data;

    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach(twc_profiles, index, item)
    {
        if (flag == TWC_ALL_PROFILES
            || (flag == TWC_LOADED_PROFILES && item->profile->tox != NULL)
            || (flag == TWC_UNLOADED_PROFILES && item->profile->tox == NULL))
        {
            weechat_hook_completion_list_add(completion,
                                             item->profile->name,
                                             0, WEECHAT_LIST_POS_SORT);
        }
    }

    return WEECHAT_RC_OK;
}

void
twc_completion_init()
{
    weechat_hook_completion("tox_profiles", "profile",
                            twc_completion_profile,
                            (void *)(intptr_t)TWC_ALL_PROFILES);
    weechat_hook_completion("tox_loaded_profiles", "loaded profile",
                            twc_completion_profile,
                            (void *)(intptr_t)TWC_LOADED_PROFILES);
    weechat_hook_completion("tox_unloaded_profiles", "unloaded profile",
                            twc_completion_profile,
                            (void *)(intptr_t)TWC_UNLOADED_PROFILES);
    weechat_hook_completion("tox_friend_tox_id", "friend Tox ID",
                            twc_completion_friend,
                            (void *)(intptr_t)TWC_COMPLETE_FRIEND_ID);
    weechat_hook_completion("tox_friend_name", "friend name",
                            twc_completion_friend,
                            (void *)(intptr_t)TWC_COMPLETE_FRIEND_NAME);
}

