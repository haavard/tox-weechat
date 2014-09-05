#include <stdlib.h>

#include <weechat/weechat-plugin.h>

#include "tox-weechat-tox.h"
#include "tox-weechat-commands.h"
#include "tox-weechat-gui.h"
#include "tox-weechat-friend-requests.h"

#include "tox-weechat.h"

WEECHAT_PLUGIN_NAME("tox");
WEECHAT_PLUGIN_DESCRIPTION("Tox protocol");
WEECHAT_PLUGIN_AUTHOR("Håvard Pettersson <haavard.pettersson@gmail.com>");
WEECHAT_PLUGIN_VERSION("0.1");
WEECHAT_PLUGIN_LICENSE("MIT");

struct t_weechat_plugin *weechat_plugin = NULL;
struct t_gui_buffer *tox_main_buffer = NULL;
int tox_weechat_online_status = 0;

int
weechat_plugin_init(struct t_weechat_plugin *plugin, int argc, char *argv[])
{
    weechat_plugin = plugin;

    tox_weechat_tox_init();
    tox_weechat_commands_init();
    tox_weechat_gui_init();
    tox_weechat_friend_requests_init();

    return WEECHAT_RC_OK;
}

int
weechat_plugin_end(struct t_weechat_plugin *plugin)
{
    tox_weechat_tox_free();
    tox_weechat_friend_requests_free();

    return WEECHAT_RC_OK;
}
