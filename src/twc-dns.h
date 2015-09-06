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

#ifndef TOX_WEECHAT_DNS_H
#define TOX_WEECHAT_DNS_H

#include <stdint.h>

#ifdef LDNS_ENABLED
#include <tox/toxdns.h>

#define TWC_DNS_ID_MAXLEN TOXDNS_MAX_RECOMMENDED_NAME_LENGTH

#endif // LDNS_ENABLED

enum t_twc_dns_rc
{
    TWC_DNS_RC_OK = 0,
    TWC_DNS_RC_ERROR = -1,
    TWC_DNS_RC_EINVAL = -2,
    TWC_DNS_RC_VERSION = -4,
};

enum t_twc_dns_rc
twc_dns_query(const char *dns_id,
              void (*callback)(void *data,
                               enum t_twc_dns_rc rc,
                               const uint8_t *tox_id),
              void *callback_data);

#endif // TOX_WEECHAT_DNS_H

