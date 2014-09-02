#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"
#include "tox-weechat-utils.h"

char *
bar_item_input_prompt(void *data,
                      struct t_gui_bar_item *item,
                      struct t_gui_window *window,
                      struct t_gui_buffer *buffer,
                      struct t_hashtable *extra_info)
{
    return tox_weechat_get_self_name_nt();
}

char *
bar_item_buffer_plugin(void *data, struct t_gui_bar_item *item,
                           struct t_gui_window *window,
                           struct t_gui_buffer *buffer,
                           struct t_hashtable *extra_info)
{
    char string[256];

    const char *name = weechat_plugin_get_name(weechat_plugin);

    const char *status = tox_weechat_online_status ? "online" : "offline";
    const char *color = weechat_color(tox_weechat_online_status ? "green" : "red");

    snprintf(string, sizeof(string),
             "%s %s%s", name, color, status);

    return strdup(string);
}

void tox_weechat_gui_init()
{
    weechat_bar_item_new("input_prompt", bar_item_input_prompt, NULL);
    weechat_bar_item_new("buffer_plugin", bar_item_buffer_plugin, NULL);
}

