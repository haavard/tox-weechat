/*
 * Copyright (c) 2018 Håvard Pettersson <mail@haavard.me>
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
#include <weechat/weechat-plugin.h>

#include "twc-profile.h"

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

char *
twc_get_peer_id_short(Tox *tox, uint32_t conference_number,
                      uint32_t peer_number);

char *
twc_get_peer_name_prefixed(const char *id, const char *name);

char *
twc_get_peer_name_prefixed_and_aligned(const char *id, const char *name,
                                       size_t max);

size_t
twc_get_max_string_length(struct t_weelist *list);

size_t
twc_get_peer_name_count(struct t_weelist *list, const char *name);

struct t_weelist *
twc_starts_with(struct t_weelist *list, const char *search,
                struct t_weelist *result);

const char *
twc_get_next_completion(struct t_weelist *completion_list,
                        const char *prev_comp);

struct t_weelist_item *
twc_is_id_ignored(struct t_twc_profile *profile, const char *short_id);

uint32_t
twc_uint32_reverse_bytes(uint32_t num);

int
twc_fit_utf8(const char *str, int max);

int
twc_set_buffer_logging(struct t_gui_buffer *buffer, bool logging);

char *
twc_tox_err_file_control(enum TOX_ERR_FILE_CONTROL error);

char *
twc_tox_err_file_get(enum TOX_ERR_FILE_GET error);

char *
twc_tox_err_file_seek(enum TOX_ERR_FILE_SEEK error);

char *
twc_tox_err_file_send(enum TOX_ERR_FILE_SEND error);

char *
twc_tox_err_file_send_chunk(enum TOX_ERR_FILE_SEND_CHUNK error);

#endif /* TOX_WEECHAT_UTILS_H */
