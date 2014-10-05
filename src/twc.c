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

#include "twc-profile.h"
#include "twc-commands.h"
#include "twc-gui.h"
#include "twc-config.h"
#include "twc-completion.h"
#include "twc-sqlite.h"

#include "twc.h"

WEECHAT_PLUGIN_NAME("tox");
WEECHAT_PLUGIN_DESCRIPTION("Tox protocol");
WEECHAT_PLUGIN_AUTHOR("Håvard Pettersson <haavard.pettersson@gmail.com>");
WEECHAT_PLUGIN_VERSION("0.1");
WEECHAT_PLUGIN_LICENSE("GPL3");

struct t_weechat_plugin *weechat_plugin = NULL;

int
weechat_plugin_init(struct t_weechat_plugin *plugin, int argc, char *argv[])
{
    weechat_plugin = plugin;

    if (twc_sqlite_init() != 0)
    {
        weechat_printf(NULL,
                       "%s%s: failed to initialize persistent storage, some "
                       "data will not be saved",
                       weechat_prefix("error"), weechat_plugin->name);
    }

    twc_profile_init();
    twc_commands_init();
    twc_gui_init();
    twc_completion_init();

    twc_config_init();
    twc_config_read();

    bool no_autoconnect = false;
    for (int i = 0; i < argc; i++)
    {
        if (weechat_strcasecmp(argv[i], "-a") == 0
            || weechat_strcasecmp(argv[i], "--no-connect") == 0)
        {
            no_autoconnect = true;
        }
    }
    if (!no_autoconnect)
        twc_profile_autoload();

    return WEECHAT_RC_OK;
}

int
weechat_plugin_end(struct t_weechat_plugin *plugin)
{
    twc_config_write();

    twc_profile_free_all();

    twc_sqlite_end();

    return WEECHAT_RC_OK;
}

