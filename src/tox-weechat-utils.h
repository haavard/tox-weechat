#ifndef TOX_WEECHAT_UTILS_H
#define TOX_WEECHAT_UTILS_H

#include <stdint.h>

/**
 * Convert a hex string to a binary address. Return value must be freed.
 */
uint8_t *tox_weechat_hex2bin(const char *hex);

/**
 * Convert a binary address to a null-terminated hex string. Must be freed.
 */
char *tox_weechat_bin2hex(const char *hex);

#endif // TOX_WEECHAT_UTILS_H
