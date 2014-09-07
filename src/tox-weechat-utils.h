#ifndef TOX_WEECHAT_UTILS_H
#define TOX_WEECHAT_UTILS_H

#include <stdlib.h>
#include <stdint.h>

#include <tox/tox.h>

void
tox_weechat_hex2bin(const char *hex, char *out);

void
tox_weechat_bin2hex(const uint8_t *bin, size_t size, char *out);

char *
tox_weechat_null_terminate(const uint8_t *str, size_t length);

char *
tox_weechat_get_name_nt(Tox *tox, int32_t friend_number);

char *
tox_weechat_get_status_message_nt(Tox *tox, int32_t friend_number);

char *
tox_weechat_get_self_name_nt(Tox *tox);

#endif // TOX_WEECHAT_UTILS_H
