/*
 * Copyright (c) 2015 Håvard Pettersson <mail@haavard.me>
 *
 * This file is part of Tox-WeeChat.
 *
 * Tox-WeeChat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Tox-WeeChat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tox-WeeChat.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TOX_WEECHAT_UTILS_H
#define TOX_WEECHAT_UTILS_H

#include <stdlib.h>

#include <tox/tox.h>

void
twc_hex2bin(const char *hex, size_t size, uint8_t *out);

void
twc_bin2hex(const uint8_t *bin, size_t size, char *out);

char *
twc_null_terminate(const uint8_t *str, size_t length);

char *
twc_get_name_nt(Tox *tox, int32_t friend_number);

char *
twc_get_status_message_nt(Tox *tox, int32_t friend_number);

char *
twc_get_peer_name_nt(Tox *tox, int32_t group_number, int32_t peer_number);

char *
twc_get_self_name_nt(Tox *tox);

char *
twc_get_friend_id_short(Tox *tox, int32_t friend_number);

uint32_t
twc_uint32_reverse_bytes(uint32_t num);

unsigned long long
twc_hash_tox_id(const uint8_t *tox_id);

enum t_twc_rc
twc_read_file(const char *path, uint8_t **data, size_t *size);

#endif // TOX_WEECHAT_UTILS_H

