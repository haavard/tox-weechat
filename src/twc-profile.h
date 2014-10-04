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

#ifndef TOX_WEECHAT_PROFILE_H
#define TOX_WEECHAT_PROFILE_H

#include <stdbool.h>

#include <tox/tox.h>

struct t_hashtable;

enum t_twc_profile_option
{
    TWC_PROFILE_OPTION_SAVEFILE = 0,
    TWC_PROFILE_OPTION_AUTOLOAD,
    TWC_PROFILE_OPTION_MAX_FRIEND_REQUESTS,

    TWC_PROFILE_NUM_OPTIONS,
};

struct t_twc_profile
{
    char *name;
    struct t_config_option *options[TWC_PROFILE_NUM_OPTIONS];

    struct Tox *tox;
    int tox_online;

    struct t_gui_buffer *buffer;
    struct t_hook *tox_do_timer;

    struct t_twc_list *chats;
    struct t_twc_list *group_chat_invites;
    struct t_hashtable *message_queues;
};

extern struct t_twc_list *twc_profiles;

void
twc_profile_init();

struct t_twc_profile *
twc_profile_new(const char *name);

void
twc_profile_load(struct t_twc_profile *profile);

void
twc_profile_unload(struct t_twc_profile *profile);

void
twc_profile_autoload();

void
twc_profile_refresh_online_status(struct t_twc_profile *profile);

void
twc_profile_set_online_status(struct t_twc_profile *profile, bool online);

struct t_twc_profile *
twc_profile_search_name(const char *name);

struct t_twc_profile *
twc_profile_search_buffer(struct t_gui_buffer *buffer);

void
twc_profile_delete(struct t_twc_profile *profile, bool delete_data);

void
twc_profile_free(struct t_twc_profile *profile);

void
twc_profile_free_all();

#endif // TOX_WEECHAT_PROFILE_H

