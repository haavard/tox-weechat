/*
 * Copyright (c) 2015 Håvard Pettersson <mail@haavard.me>
 *
 * This file is part of Tox-WeeChat.
 *
 * Tox-WeeChat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or * (at your option) any later version.
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
#include "twc-profile.h"
#include "twc-utils.h"

#include "twc-gui.h"

char *
twc_bar_item_away(void *data,
                  struct t_gui_bar_item *item,
                  struct t_gui_window *window,
                  struct t_gui_buffer *buffer,
                  struct t_hashtable *extra_info)
{
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);

    if (!profile || !(profile->tox))
        return NULL;

    char *status;
    switch (tox_self_get_status(profile->tox))
    {
        case TOX_USER_STATUS_NONE:
            status = NULL;
            break;
        case TOX_USER_STATUS_BUSY:
            status = strdup("busy");
            break;
        case TOX_USER_STATUS_AWAY:
            status = strdup("away");
            break;
    }

    return status;
}

char *
twc_bar_item_input_prompt(void *data,
                          struct t_gui_bar_item *item,
                          struct t_gui_window *window,
                          struct t_gui_buffer *buffer,
                          struct t_hashtable *extra_info)
{
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);

    if (!profile || !(profile->tox))
        return NULL;

    return twc_get_self_name_nt(profile->tox);
}

char *
twc_bar_item_buffer_plugin(void *data, struct t_gui_bar_item *item,
                           struct t_gui_window *window,
                           struct t_gui_buffer *buffer,
                           struct t_hashtable *extra_info)
{
    struct t_twc_profile *profile = twc_profile_search_buffer(buffer);

    char string[256];

    const char *plugin_name = weechat_plugin_get_name(weechat_plugin);

    if (!profile)
        return strdup(plugin_name);

    const char *profile_name = profile->name;

    snprintf(string, sizeof(string),
             "%s%s/%s%s%s/%s%s",
             plugin_name,
             weechat_color("bar_delim"),
             weechat_color("bar_fg"),
             profile_name,
             weechat_color("bar_delim"),
             weechat_color("bar_fg"),
             profile->tox_online ? "online" : "offline");

    return strdup(string);
}

void twc_gui_init()
{
    weechat_bar_item_new("away", twc_bar_item_away, NULL);
    weechat_bar_item_new("input_prompt", twc_bar_item_input_prompt, NULL);
    weechat_bar_item_new("buffer_plugin", twc_bar_item_buffer_plugin, NULL);
}

