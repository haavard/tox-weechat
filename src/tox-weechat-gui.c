#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"
#include "tox-weechat-utils.h"

char *bar_item_input_prompt(void *data,
                            struct t_gui_bar_item *item,
                            struct t_gui_window *window,
                            struct t_gui_buffer *buffer,
                            struct t_hashtable *extra_info)
{
    return tox_weechat_get_self_name_nt();
}

void tox_weechat_gui_init()
{
    weechat_bar_item_new("input_prompt", bar_item_input_prompt, NULL);
}

