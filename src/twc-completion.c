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

#include <weechat/weechat-plugin.h>

#include "twc.h"
#include "twc-list.h"
#include "twc-profile.h"

#include "twc-completion.h"

enum
{
    TWC_ALL_PROFILES,
    TWC_LOADED_PROFILES,
    TWC_UNLOADED_PROFILES,
};

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
                                             0,
                                             WEECHAT_LIST_POS_SORT);
        }
    }

    return WEECHAT_RC_OK;
}

void
twc_completion_init()
{
    weechat_hook_completion("tox_profiles",
                            "profile",
                            twc_completion_profile,
                            (void *)(intptr_t)TWC_ALL_PROFILES);
    weechat_hook_completion("tox_loaded_profiles",
                            "profile",
                            twc_completion_profile,
                            (void *)(intptr_t)TWC_LOADED_PROFILES);
    weechat_hook_completion("tox_unloaded_profiles",
                            "profile",
                            twc_completion_profile,
                            (void *)(intptr_t)TWC_UNLOADED_PROFILES);
}

