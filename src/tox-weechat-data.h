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

#ifndef TOX_WEECHAT_DATA_H
#define TOX_WEECHAT_DATA_H

struct t_tox_weechat_identity;

void
tox_weechat_data_load();

void
tox_weechat_data_identity_save(struct t_tox_weechat_identity *identity);

void
tox_weechat_data_identity_load(struct t_tox_weechat_identity *identity);

int
tox_weechat_data_save();

void
tox_weechat_data_free();

#endif // TOX_WEECHAT_DATA_H
