#ifndef TOX_WEECHAT_CONFIG_H
#define TOX_WEECHAT_CONFIG_H

#include "tox-weechat-identities.h"

extern struct t_config_file *tox_weechat_config_file;
extern struct t_config_section *tox_weechat_config_section_identity;

void
tox_weechat_config_init();

int
tox_weechat_config_read();

int
tox_weechat_config_write();

void
tox_weechat_config_init_identity(struct t_tox_weechat_identity *identity);

#endif // TOX_WEECHAT_CONFIG_H
