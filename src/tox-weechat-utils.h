#ifndef TOX_WEECHAT_UTILS_H
#define TOX_WEECHAT_UTILS_H

#include <stdlib.h>
#include <stdint.h>

/**
 * Convert a hex string to a binary address. Return value must be freed.
 */
uint8_t *tox_weechat_hex2bin(const char *hex);

/**
 * Convert a binary address to a null-terminated hex string. Must be freed.
 */
char *tox_weechat_bin2hex(const uint8_t *bin, size_t size);

/**
 * Return a new string that is a null-terminated version of the argument. Must
 * be freed.
 */
char *tox_weechat_null_terminate(const uint8_t *str, size_t length);

/**
 * Get the name of a friend as a null-terminated string. Must be freed.
 */
char *tox_weechat_get_name_nt(int32_t friend_number);

/**
 * Get the status message of a friend as a null-terminated string. Must be
 * freed.
 */
char *tox_weechat_get_status_message_nt(int32_t friend_number);

/**
 * Get the name of the user as a null-terminated string. Must be freed.
 */
char *tox_weechat_get_self_name_nt();

#endif // TOX_WEECHAT_UTILS_H
