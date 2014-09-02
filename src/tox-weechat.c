#include <weechat/weechat-plugin.h>

WEECHAT_PLUGIN_NAME("tox");
WEECHAT_PLUGIN_DESCRIPTION("Tox protocol");
WEECHAT_PLUGIN_AUTHOR("Håvard Pettersson <haavard.pettersson@gmail.com>");
WEECHAT_PLUGIN_VERSION("0.1");
WEECHAT_PLUGIN_LICENSE("GPL3");

int
weechat_plugin_init(struct t_weechat_plugin *plugin,
                    int argc, char *argv[])
{
    return WEECHAT_RC_OK;
}

int
weechat_plugin_end(struct t_weechat_plugin *plugin)
{
    return WEECHAT_RC_OK;
}
