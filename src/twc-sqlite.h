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

#ifndef TWC_SQLITE_H
#define TWC_SQLITE_H

#include <stdint.h>

struct t_twc_profile;
struct t_twc_friend_request;

int
twc_sqlite_init();

int
twc_sqlite_add_profile(struct t_twc_profile *profile);

int
twc_sqlite_delete_profile(struct t_twc_profile *profile);

int
twc_sqlite_add_friend_request(struct t_twc_profile *profile,
                              struct t_twc_friend_request *friend_request);

int
twc_sqlite_friend_request_count(struct t_twc_profile *profile);

struct t_twc_list *
twc_sqlite_friend_requests(struct t_twc_profile *profile);

struct t_twc_friend_request *
twc_sqlite_friend_request_with_id(struct t_twc_profile *profile,
                                  int64_t id);

int
twc_sqlite_delete_friend_request_with_id(struct t_twc_profile *profile,
                                         int64_t id);

void
twc_sqlite_end();

#endif // TWC_SQLITE_H
