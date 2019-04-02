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

#ifndef TOX_WEECHAT_CHAT_H
#define TOX_WEECHAT_CHAT_H

#include <stdbool.h>
#include <stdint.h>

struct t_twc_list;

extern const char *twc_tag_unsent_message;
extern const char *twc_tag_sent_message;
extern const char *twc_tag_received_message;

struct t_twc_chat
{
    struct t_twc_profile *profile;

    struct t_gui_buffer *buffer;
    int32_t friend_number;
    int32_t group_number;

    struct t_gui_nick_group *nicklist_group;
    struct t_weelist *nicks;
    struct t_weelist *ids;
    struct t_weelist *completion;
    char *last_search;
    char *prev_comp;
};

struct t_twc_chat *
twc_chat_search_friend(struct t_twc_profile *profile, int32_t friend_number,
                       bool create_new);

struct t_twc_chat *
twc_chat_search_group(struct t_twc_profile *profile, int32_t group_number,
                      bool create_new);

struct t_twc_chat *
twc_chat_search_buffer(struct t_gui_buffer *target_buffer);

void
twc_chat_update_prefix(struct t_twc_chat *chat, const char *id,
                       const char *prefix, const char *prefix_color);

void
twc_chat_update_prefix_by_nick(struct t_gui_buffer *buffer,
                               struct t_gui_nick *nick, const char *prefix,
                               const char *prefix_color);

enum t_twc_rc
twc_chat_set_logging(struct t_twc_chat const *const chat, bool logging);

void
twc_chat_print_message(struct t_twc_chat *chat, const char *tags,
                       const char *color, const char *sender,
                       const char *message, TOX_MESSAGE_TYPE message_type);

void
twc_chat_send_message(struct t_twc_chat *chat, const char *message,
                      TOX_MESSAGE_TYPE message_type);

void
twc_chat_queue_refresh(struct t_twc_chat *chat);

void
twc_chat_free(struct t_twc_chat *chat);

void
twc_chat_free_list(struct t_twc_list *list);

#endif /* TOX_WEECHAT_CHAT_H */
