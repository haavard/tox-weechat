#ifndef TOX_WEECHAT_H
#define TOX_WEECHAT_H

#include <tox/tox.h>

extern struct t_weechat_plugin *weechat_plugin;
extern Tox *tox;
extern int tox_weechat_online_status;

extern struct t_gui_buffer *tox_main_buffer;

#endif // TOX_WEECHAT_H
