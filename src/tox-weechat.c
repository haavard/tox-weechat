#include <stdlib.h>

#include <weechat/weechat-plugin.h>

#include "tox-weechat-tox.h"

#include "tox-weechat.h"

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

    tox_weechat_tox_init();

    return WEECHAT_RC_OK;
}

int
weechat_plugin_end(struct t_weechat_plugin *plugin)
{
    return WEECHAT_RC_OK;
}
