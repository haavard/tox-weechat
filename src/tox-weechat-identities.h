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

#ifndef TOX_WEECHAT_IDENTITIES_H
#define TOX_WEECHAT_IDENTITIES_H

#include <stdint.h>
#include <stdbool.h>

#include <tox/tox.h>

enum t_tox_weechat_identity_option
{
    TOX_WEECHAT_IDENTITY_OPTION_SAVEFILE = 0,
    TOX_WEECHAT_IDENTITY_OPTION_AUTOCONNECT,
    TOX_WEECHAT_IDENTITY_OPTION_MAX_FRIEND_REQUESTS,

    TOX_WEECHAT_IDENTITY_NUM_OPTIONS,
};

struct t_tox_weechat_identity
{
    char *name;
    struct t_config_option *options[TOX_WEECHAT_IDENTITY_NUM_OPTIONS];

    struct Tox *tox;
    int tox_online;

    struct t_gui_buffer *buffer;
    struct t_hook *tox_do_timer;

    struct t_tox_weechat_chat *chats;
    struct t_tox_weechat_chat *last_chat;

    struct t_tox_weechat_friend_request *friend_requests;
    struct t_tox_weechat_friend_request *last_friend_request;
    unsigned int friend_request_count;

    struct t_tox_weechat_unsent_message_recipient *unsent_message_recipients;
    struct t_tox_weechat_unsent_message_recipient *last_unsent_message_recipient;

    struct t_tox_weechat_identity *next_identity;
    struct t_tox_weechat_identity *prev_identity;
};

extern struct t_tox_weechat_identity *tox_weechat_identities;
extern struct t_tox_weechat_identity *tox_weechat_last_identity;

struct t_tox_weechat_identity *
tox_weechat_identity_new(const char *name);

void
tox_weechat_identity_connect(struct t_tox_weechat_identity *identity);

void
tox_weechat_identity_disconnect(struct t_tox_weechat_identity *identity);

void
tox_weechat_identity_autoconnect();

void
tox_weechat_identity_refresh_online_status(struct t_tox_weechat_identity *identity);

void
tox_weechat_identity_set_online_status(struct t_tox_weechat_identity *identity,
                                       bool online);

struct t_tox_weechat_identity *
tox_weechat_identity_name_search(const char *name);

struct t_tox_weechat_identity *
tox_weechat_identity_for_buffer(struct t_gui_buffer *buffer);

void
tox_weechat_identity_delete(struct t_tox_weechat_identity *identity,
                            bool delete_data);

void
tox_weechat_identity_free(struct t_tox_weechat_identity *identity);

void
tox_weechat_identity_free_all();

#endif // TOX_WEECHAT_IDENTITIES_H
