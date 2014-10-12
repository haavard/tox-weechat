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

#include "twc-list.h"

/**
 * Create and return a new list.
 */
struct t_twc_list *
twc_list_new()
{
    struct t_twc_list *list = malloc(sizeof(struct t_twc_list));

    list->head = list->tail = NULL;
    list->count = 0;

    return list;
}

/**
 * Create and return a new list item.
 */
struct t_twc_list_item *
twc_list_item_new()
{
    struct t_twc_list_item *item = malloc(sizeof(struct t_twc_list_item));

    return item;
}

/**
 * Create and return a new list item with data.
 */
struct t_twc_list_item *
twc_list_item_new_data(const void *data)
{
    struct t_twc_list_item *item = twc_list_item_new();
    item->data = (void *)data;

    return item;
}

/**
 * Create a new list item, add it to a list and return the item.
 */
struct t_twc_list_item *
twc_list_item_new_add(struct t_twc_list *list)
{
    struct t_twc_list_item *item = twc_list_item_new();
    twc_list_add(list, item);
    return item;
}

/**
 * Create a new list item with data, add it to a list and return the item.
 */
struct t_twc_list_item *
twc_list_item_new_data_add(struct t_twc_list *list, const void *data)
{
    struct t_twc_list_item *item = twc_list_item_new_data(data);
    twc_list_add(list, item);
    return item;
}

/**
 * Add an item to the list.
 */
void
twc_list_add(struct t_twc_list *list,
             struct t_twc_list_item *item)
{
    item->list = list;

    item->prev_item = list->tail;
    item->next_item = NULL;

    if (list->head == NULL)
        list->head = item;
    else
        list->tail->next_item = item;

    list->tail = item;

    ++(list->count);
}

/**
 * Remove an item from the list it's in. Frees the item, but not the data
 * associated with it.
 *
 * Returns the data of the removed item.
 */
void *
twc_list_remove(struct t_twc_list_item *item)
{
    struct t_twc_list *list = item->list;

    if (item == list->tail)
        list->tail = item->prev_item;

    if (item->prev_item)
        item->prev_item->next_item = item->next_item;
    else
        list->head = item->next_item;

    if (item->next_item)
        item->next_item->prev_item = item->prev_item;

    --(list->count);

    void *data = item->data;

    free(item);

    return data;
}

/**
 * Remove an item with the given data from the list. Frees the item, but not
 * the data.
 */
void
twc_list_remove_with_data(struct t_twc_list *list, const void *data)
{
    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach(list, index, item)
    {
        if (item->data == data)
        {
            twc_list_remove(item);
            break;
        }
    }
}

/**
 * Remove the last item from the list. Frees the item, and returns the data
 * associated with it.
 */
void *
twc_list_pop(struct t_twc_list *list)
{
    if (list->tail)
        return twc_list_remove(list->tail);
    else
        return NULL;
}

/**
 * Return the list item at an index, or NULL if it does not exist.
 */
struct t_twc_list_item *
twc_list_get(struct t_twc_list *list, size_t index)
{
    if (index > list->count)
        return NULL;

    size_t current_index;
    struct t_twc_list_item *item;
    if (list->count - index > index / 2)
    {
        twc_list_foreach(list, current_index, item)
        {
            if (current_index == index)
                return item;
        }
    }
    else
    {
        twc_list_foreach_reverse(list, current_index, item)
        {
            if (current_index == index)
                return item;
        }
    }

    return NULL;
}

