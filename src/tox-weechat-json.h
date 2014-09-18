/*
 * Copyright (c) 2014 Håvard Pettersson <haavard.pettersson@gmail.com>
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

#ifndef TOX_WEECHAT_JSON_H
#define TOX_WEECHAT_JSON_H

#include <jansson.h>

#include "tox-weechat-identities.h"

extern json_t *tox_weechat_json_config;

extern const char *tox_weechat_json_key_friend_requests;
extern const char *tox_weechat_json_friend_request_key_client_id;
extern const char *tox_weechat_json_friend_request_key_message;

void
tox_weechat_json_init();

int
tox_weechat_json_save();

json_t *
tox_weechat_json_get_identity_object(struct t_tox_weechat_identity *identity);

int
tox_weechat_json_set_identity_object(struct t_tox_weechat_identity *identity,
                                     json_t *object);

#endif // TOX_WEECHAT_JSON_H
