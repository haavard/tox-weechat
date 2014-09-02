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
irc_bar_item_buffer_plugin(void *data, struct t_gui_bar_item *item,
                           struct t_gui_window *window,
                           struct t_gui_buffer *buffer,
                           struct t_hashtable *extra_info)
{
    char string[512];

    const char *name = weechat_plugin_get_name(weechat_plugin);

    char *status = "online" 



            snprintf(string, sizeof(string), "%s %s%s",
                      name,
                      weechat_color("
                      );
        }
        else
        {
            snprintf (buf, sizeof (buf), "%s", name);
        }
    }
    else
    {
        snprintf (buf, sizeof (buf), "%s", name);
    }
    return strdup (buf);
}

void tox_weechat_gui_init()
{
    weechat_bar_item_new("input_prompt", bar_item_input_prompt, NULL);
}

