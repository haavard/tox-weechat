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

#ifndef TOX_WEECHAT_CONFIG_H
#define TOX_WEECHAT_CONFIG_H

struct t_twc_profile;

extern struct t_config_option *twc_config_friend_request_message;
extern struct t_config_option *twc_config_short_id_size;

enum t_twc_proxy
{
    TWC_PROXY_NONE = 0,
    TWC_PROXY_SOCKS5,
    TWC_PROXY_HTTP
};

void
twc_config_init();

int
twc_config_read();

int
twc_config_write();

void
twc_config_init_profile(struct t_twc_profile *profile);

#endif // TOX_WEECHAT_CONFIG_H

