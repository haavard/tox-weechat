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

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "twc.h"
#include "twc-utils.h"

#include "twc-dns.h"

/**
 * Struct for holding information about a Tox DNS query callback.
 */
struct t_twc_dns_callback_info
{
    void (*callback)(void *data,
                     enum t_twc_dns_rc rc,
                     const uint8_t *tox_id);
    void *data;

    struct t_hook *hook;
};

/**
 * Callback when the DNS resolver child process has written to our file
 * descriptor. Process the data a bit and pass it on to the original callback.
 */
int
twc_dns_fd_callback(void *data, int fd)
{
    struct t_twc_dns_callback_info *callback_info = data;

    char buffer[TOX_FRIEND_ADDRESS_SIZE * 2 + 1];
    ssize_t size = read(fd, buffer, sizeof(buffer) - 1);
    buffer[size] = '\0';

    if (size > 0)
    {
        if (size == TOX_FRIEND_ADDRESS_SIZE * 2)
        {
            uint8_t tox_id[TOX_FRIEND_ADDRESS_SIZE];
            twc_hex2bin(buffer, TOX_FRIEND_ADDRESS_SIZE, tox_id);

            callback_info->callback(callback_info->data,
                                    TWC_DNS_RC_OK, tox_id);
        }
        else
        {
            int rc = atoi(buffer);
            callback_info->callback(callback_info->data,
                                    rc, NULL);
        }

        weechat_unhook(callback_info->hook);
        free(callback_info);
    }

    return WEECHAT_RC_OK;
}

/**
 * Perform a Tox DNS lookup and write either a Tox ID (as a hexadecimal string)
 * or an error code (as a decimal string) to out_fd.
 */
void
twc_perform_dns_lookup(const char *dns_id, int out_fd)
{
    dprintf(out_fd, "%d", TWC_DNS_RC_ERROR);
}

/**
 * Perform a Tox DNS lookup. If return code (rc) parameter in callback is
 * TWC_DNS_RC_OK, tox_id contains the resolved Tox ID. For any other value,
 * something caused the Tox ID not to be found.
 *
 * If twc_dns_query returns TWC_DNS_RC_ERROR, the callback will never be
 * called.
 */
enum t_twc_dns_rc
twc_dns_query(const char *dns_id,
              void (*callback)(void *data,
                               enum t_twc_dns_rc rc,
                               const uint8_t *tox_id),
              void *callback_data)
{
    if (!callback) return TWC_DNS_RC_OK;

    int fifo[2];
    if (pipe(fifo) < 0)
        return TWC_DNS_RC_ERROR;

    pid_t pid;
    if ((pid = fork()) == -1)
    {
        return TWC_DNS_RC_ERROR;
    }
    else if (pid == 0)
    {
        twc_perform_dns_lookup(dns_id, fifo[1]);
        _exit(0);
    }
    else
    {
        struct t_twc_dns_callback_info *callback_info
            = malloc(sizeof(struct t_twc_dns_callback_info));
        if (!callback_info)
            return TWC_DNS_RC_ERROR;

        callback_info->callback = callback;
        callback_info->data = callback_data;
        callback_info->hook = weechat_hook_fd(fifo[0], 1, 0, 0,
                                              twc_dns_fd_callback,
                                              callback_info);
    }

    return TWC_DNS_RC_OK;
}

