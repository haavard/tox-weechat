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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"
#include "tox-weechat-identities.h"
#include "tox-weechat-utils.h"

#include "tox-weechat-gui.h"

char *
bar_item_away(void *data,
              struct t_gui_bar_item *item,
              struct t_gui_window *window,
              struct t_gui_buffer *buffer,
              struct t_hashtable *extra_info)
{
    struct t_tox_weechat_identity *identity = tox_weechat_identity_for_buffer(buffer);

    if (!identity)
        return NULL;

    char *status = NULL;;
    switch (tox_get_self_user_status(identity->tox))
    {
        case TOX_USERSTATUS_BUSY:
            status = strdup("busy");
            break;
        case TOX_USERSTATUS_AWAY:
            status = strdup("away");
            break;
    }

    return status;
}

char *
bar_item_input_prompt(void *data,
                      struct t_gui_bar_item *item,
                      struct t_gui_window *window,
                      struct t_gui_buffer *buffer,
                      struct t_hashtable *extra_info)
{
    struct t_tox_weechat_identity *identity = tox_weechat_identity_for_buffer(buffer);
    return tox_weechat_get_self_name_nt(identity->tox);
}

char *
bar_item_buffer_plugin(void *data, struct t_gui_bar_item *item,
                       struct t_gui_window *window,
                       struct t_gui_buffer *buffer,
                       struct t_hashtable *extra_info)
{
    struct t_tox_weechat_identity *identity = tox_weechat_identity_for_buffer(buffer);

    char string[256];

    const char *plugin_name = weechat_plugin_get_name(weechat_plugin);
    const char *identity_name = identity->name;

    snprintf(string, sizeof(string),
             "%s%s/%s%s%s/%s%s",
             plugin_name,
             weechat_color("bar_delim"),
             weechat_color("bar_fg"),
             identity_name,
             weechat_color("bar_delim"),
             weechat_color("bar_fg"),
             identity->tox_online ? "online" : "offline");

    return strdup(string);
}

void tox_weechat_gui_init()
{
    weechat_bar_item_new("away", bar_item_away, NULL);
    weechat_bar_item_new("input_prompt", bar_item_input_prompt, NULL);
    weechat_bar_item_new("buffer_plugin", bar_item_buffer_plugin, NULL);
}

