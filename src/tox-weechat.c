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

#include <weechat/weechat-plugin.h>

#include "tox-weechat-identities.h"
#include "tox-weechat-commands.h"
#include "tox-weechat-gui.h"
#include "tox-weechat-friend-requests.h"
#include "tox-weechat-config.h"
#include "tox-weechat-data.h"
#include "tox-weechat-completion.h"

#include "tox-weechat.h"

WEECHAT_PLUGIN_NAME("tox");
WEECHAT_PLUGIN_DESCRIPTION("Tox protocol");
WEECHAT_PLUGIN_AUTHOR("Håvard Pettersson <haavard.pettersson@gmail.com>");
WEECHAT_PLUGIN_VERSION("0.1");
WEECHAT_PLUGIN_LICENSE("GPL3");

struct t_weechat_plugin *weechat_plugin = NULL;
struct t_gui_buffer *tox_main_buffer = NULL;
int tox_weechat_online_status = 0;

int
weechat_plugin_init(struct t_weechat_plugin *plugin, int argc, char *argv[])
{
    weechat_plugin = plugin;

    tox_weechat_config_init();
    tox_weechat_config_read();
    tox_weechat_json_load();
    tox_weechat_commands_init();
    tox_weechat_gui_init();
    tox_weechat_completion_init();

    tox_weechat_identity_autoconnect();

    return WEECHAT_RC_OK;
}

int
weechat_plugin_end(struct t_weechat_plugin *plugin)
{
    tox_weechat_config_write();
    tox_weechat_json_save();
    tox_weechat_identity_free_all();

    return WEECHAT_RC_OK;
}
