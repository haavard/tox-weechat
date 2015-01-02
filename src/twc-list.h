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

#ifndef TOX_WEECHAT_LIST_H
#define TOX_WEECHAT_LIST_H

#include <stdlib.h>

struct t_twc_list
{
    size_t count;
    struct t_twc_list_item *head;
    struct t_twc_list_item *tail;
};

struct t_twc_list_item
{
    struct t_twc_list *list;

    // don't know if this is a good idea
    // probably not
    union
    {
        void *data;
        struct t_twc_profile *profile;
        struct t_twc_friend_request *friend_request;
        struct t_twc_group_chat_invite *group_chat_invite;
        struct t_twc_chat *chat;
        struct t_twc_queued_message *queued_message;
    };

    struct t_twc_list_item *next_item;
    struct t_twc_list_item *prev_item;
};

struct t_twc_list *
twc_list_new();

struct t_twc_list_item *
twc_list_item_new();

struct t_twc_list_item *
twc_list_item_new_data(const void *data);

struct t_twc_list_item *
twc_list_item_new_add(struct t_twc_list *list);

struct t_twc_list_item *
twc_list_item_new_data_add(struct t_twc_list *list, const void *data);

void
twc_list_add(struct t_twc_list *list, struct t_twc_list_item *item);

void *
twc_list_remove(struct t_twc_list_item *item);

void
twc_list_remove_with_data(struct t_twc_list *list, const void *data);

void *
twc_list_pop(struct t_twc_list *list);

struct t_twc_list_item *
twc_list_get(struct t_twc_list *list, size_t index);

#define twc_list_foreach(list, index, item) \
    for (item = list->head, index = 0; \
         item; \
         item = item->next_item, ++index)

#define twc_list_foreach_reverse(list, index, item) \
    for (item = list->tail, index = list->count - 1; \
         item; \
         item = item->prev_item, --index)

#endif // TOX_WEECHAT_LIST_H

