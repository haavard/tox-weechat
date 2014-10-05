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

#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "twc.h"
#include "twc-list.h"
#include "twc-bootstrap.h"
#include "twc-config.h"
#include "twc-friend-request.h"
#include "twc-group-invite.h"
#include "twc-message-queue.h"
#include "twc-chat.h"
#include "twc-tox-callbacks.h"
#include "twc-sqlite.h"
#include "twc-utils.h"

#include "twc-profile.h"

struct t_twc_list *twc_profiles = NULL;

/**
 * Get a profile's expanded data path, replacing:
 *  - %h with WeeChat home
 *  - %p with profile name
 *
 * Returned string must be freed.
 */
char *
twc_profile_expanded_data_path(struct t_twc_profile *profile)
{
    const char *weechat_dir = weechat_info_get ("weechat_dir", NULL);
    const char *base_path = weechat_config_string(profile->options[TWC_PROFILE_OPTION_SAVEFILE]);
    char *home_expanded = weechat_string_replace(base_path, "%h", weechat_dir);
    char *full_path = weechat_string_replace(home_expanded, "%p", profile->name);
    free(home_expanded);

    return full_path;
}

/**
 * Try loading a Tox save file for a profile.
 *
 * Returns 0 on success, -1 on failure.
 *
 * TODO: support encrypted save files
 */
int
twc_profile_load_data(struct t_twc_profile *profile)
{
    char *full_path = twc_profile_expanded_data_path(profile);
    FILE *file = fopen(full_path, "r");
    free(full_path);

    if (file)
    {
        // get file size
        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        rewind(file);

        // allocate a buffer and read file into it
        uint8_t *data = malloc(sizeof(*data) * size);
        fread(data, sizeof(uint8_t), size, file);
        fclose(file);

        // try loading the data
        int status = tox_load(profile->tox, data, size);
        free(data);

        return status;
    }

    return -1;
}

/**
 * Save a profile's Tox data to disk.
 *
 * Returns 0 on success, -1 on failure.
 *
 * TODO: support encrypted save files
 */
int
twc_save_profile_data_file(struct t_twc_profile *profile)
{
    if (!(profile->tox))
        return -1;

    char *full_path = twc_profile_expanded_data_path(profile);

    // create containing folder if it doesn't exist
    char *rightmost_slash = strrchr(full_path, '/');
    char *dir_path = weechat_strndup(full_path, rightmost_slash - full_path);
    weechat_mkdir_parents(dir_path, 0755);
    free(dir_path);

    // save Tox data to a buffer
    uint32_t size = tox_size(profile->tox);
    uint8_t *data = malloc(size);
    tox_save(profile->tox, data);

    // save buffer to a file
    FILE *file = fopen(full_path, "w");
    if (file)
    {
        size_t saved_size = fwrite(data, sizeof(data[0]), size, file);
        fclose(file);

        return saved_size == size;
    }

    return -1;
}

/**
 * Callback when a profile's main buffer is closed. Unloads the profile.
 */
int
twc_profile_buffer_close_callback(void *data,
                                  struct t_gui_buffer *buffer)
{
    struct t_twc_profile *profile = data;

    profile->buffer = NULL;
    twc_profile_unload(profile);

    return WEECHAT_RC_OK;
}

/**
 * Initialize the Tox profiles list.
 */
void
twc_profile_init()
{
    twc_profiles = twc_list_new();
}

/**
 * Create a new profile object and add it to the list of profiles.
 */
struct t_twc_profile *
twc_profile_new(const char *name)
{
    struct t_twc_profile *profile = malloc(sizeof(struct t_twc_profile));
    profile->name = strdup(name);

    // add to profile list
    twc_list_item_new_data_add(twc_profiles, profile);

    // set up internal vars
    profile->tox = NULL;
    profile->buffer = NULL;
    profile->tox_do_timer = NULL;
    profile->tox_online = false;

    profile->chats = twc_list_new();
    profile->group_chat_invites = twc_list_new();
    profile->message_queues = weechat_hashtable_new(32,
                                                    WEECHAT_HASHTABLE_INTEGER,
                                                    WEECHAT_HASHTABLE_POINTER,
                                                    NULL, NULL);

    // set up config
    twc_config_init_profile(profile);

    return profile;
}

/**
 * Load a profile's Tox object, creating a new one if it can't be loaded from
 * disk, and bootstraps the Tox DHT.
 */
void
twc_profile_load(struct t_twc_profile *profile)
{
    if (profile->tox)
        return;

    if (!(profile->buffer))
    {
        // create main buffer
        profile->buffer = weechat_buffer_new(profile->name,
                                             NULL, NULL,
                                             twc_profile_buffer_close_callback, profile);
        if (!(profile->buffer))
            return;
    }

    weechat_printf(profile->buffer,
                   "%s%s: profile %s connecting",
                   weechat_prefix("network"), weechat_plugin->name,
                   profile->name);

    // create Tox
    profile->tox = tox_new(NULL);
    if (!(profile->tox))
    {
        weechat_printf(profile->buffer,
                       "Could not create Tox instance!",
                       weechat_prefix("error"), weechat_plugin->name);
        return;
    }

    // try loading Tox saved data
    // TODO: this can return -1 even if it does not fail
    if (twc_profile_load_data(profile) == -1)
    {
        // we failed to load - set some defaults
        char *name;
        struct passwd *user_pwd;
        if ((user_pwd = getpwuid(geteuid())))
            name = user_pwd->pw_name;
        else
            name = "Tox-WeeChat User";

        tox_set_name(profile->tox,
                     (uint8_t *)name, strlen(name));
    }

    // register with sqlite
    twc_sqlite_add_profile(profile);

    int friend_request_count = twc_sqlite_friend_request_count(profile);
    if (friend_request_count > 0)
    {
        weechat_printf(profile->buffer,
                       "%sYou have %d pending friend requests.",
                       weechat_prefix("network"),
                       friend_request_count);
    }

    // bootstrap DHT
    // TODO: add count to config
    int bootstrap_node_count = 5;
    for (int i = 0; i < bootstrap_node_count; ++i)
        twc_bootstrap_random_node(profile->tox);

    // start Tox_do loop
    twc_do_timer_cb(profile, 0);

    // register Tox callbacks
    tox_callback_friend_message(profile->tox, twc_friend_message_callback, profile);
    tox_callback_friend_action(profile->tox, twc_friend_action_callback, profile);
    tox_callback_connection_status(profile->tox, twc_connection_status_callback, profile);
    tox_callback_name_change(profile->tox, twc_name_change_callback, profile);
    tox_callback_user_status(profile->tox, twc_user_status_callback, profile);
    tox_callback_status_message(profile->tox, twc_status_message_callback, profile);
    tox_callback_friend_request(profile->tox, twc_friend_request_callback, profile);
    tox_callback_group_invite(profile->tox, twc_group_invite_callback, profile);
    tox_callback_group_message(profile->tox, twc_group_message_callback, profile);
    tox_callback_group_action(profile->tox, twc_group_action_callback, profile);
    tox_callback_group_namelist_change(profile->tox, twc_group_namelist_change_callback, profile);
}

/**
 * Unload a Tox profile. Disconnects from the network, saves data to disk.
 */
void
twc_profile_unload(struct t_twc_profile *profile)
{
    // check that we're not already disconnected
    if (!(profile->tox))
        return;

    // save and kill tox
    int result = twc_save_profile_data_file(profile);
    tox_kill(profile->tox);
    profile->tox = NULL;

    if (result == -1)
    {
        char *path = twc_profile_expanded_data_path(profile);
        weechat_printf(NULL,
                       "%s%s: Could not save Tox data for profile %s to file: %s",
                       weechat_prefix("error"), weechat_plugin->name,
                       profile->name,
                       path);
        free(path);
    }

    // stop Tox timer
    weechat_unhook(profile->tox_do_timer);

    // have to refresh and hide bar items even if we were already offline
    // TODO
    twc_profile_refresh_online_status(profile);
    twc_profile_set_online_status(profile, false);
}

/**
 * Load profiles that should autoload.
 */
void
twc_profile_autoload()
{
    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach(twc_profiles, index, item)
    {
        if (weechat_config_boolean(item->profile->options[TWC_PROFILE_OPTION_AUTOLOAD]))
            twc_profile_load(item->profile);
    }
}

void
twc_profile_refresh_online_status(struct t_twc_profile *profile)
{
    weechat_bar_item_update("buffer_plugin");
    weechat_bar_item_update("input_prompt");
    weechat_bar_item_update("away");
}

void
twc_profile_set_online_status(struct t_twc_profile *profile,
                              bool status)
{
    if (profile->tox_online ^ status)
    {
        profile->tox_online = status;
        twc_profile_refresh_online_status(profile);

        if (profile->tox_online)
        {
            weechat_printf(profile->buffer,
                           "%s%s: profile %s connected",
                           weechat_prefix("network"),
                           weechat_plugin->name,
                           profile->name);
        }
        else
        {
            weechat_printf(profile->buffer,
                           "%s%s: profile %s disconnected",
                           weechat_prefix("network"),
                           weechat_plugin->name,
                           profile->name);
        }
    }
}

/**
 * Return the profile with a certain name. Case insensitive.
 */
struct t_twc_profile *
twc_profile_search_name(const char *name)
{
    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach(twc_profiles, index, item)
    {
        if (weechat_strcasecmp(item->profile->name, name) == 0)
            return item->profile;
    }

    return NULL;
}

/**
 * Return the profile associated with a buffer, if any.
 */
struct t_twc_profile *
twc_profile_search_buffer(struct t_gui_buffer *buffer)
{
    size_t profile_index;
    struct t_twc_list_item *profile_item;
    twc_list_foreach(twc_profiles, profile_index, profile_item)
    {
        if (profile_item->profile->buffer == buffer)
            return profile_item->profile;

        size_t chat_index;
        struct t_twc_list_item *chat_item;
        twc_list_foreach(profile_item->profile->chats, chat_index, chat_item)
        {
            if (chat_item->chat->buffer == buffer)
                return profile_item->profile;
        }
    }

    return NULL;
}

/**
 * Delete a profile. Unloads, frees and deletes everything. If delete_data is
 * true, Tox data on disk is also deleted.
 */
void
twc_profile_delete(struct t_twc_profile *profile,
                   bool delete_data)
{
    char *data_path = twc_profile_expanded_data_path(profile);

    twc_sqlite_delete_profile(profile);
    twc_profile_free(profile);

    if (delete_data)
        unlink(data_path);
}

/**
 * Frees a profile. Unloads and frees all variables.
 */
void
twc_profile_free(struct t_twc_profile *profile)
{
    // unload if needed
    twc_profile_unload(profile);

    // close buffer
    if (profile->buffer)
    {
        weechat_buffer_set_pointer(profile->buffer, "close_callback", NULL);
        weechat_buffer_close(profile->buffer);
    }

    // free things
    twc_chat_free_list(profile->chats);
    twc_group_chat_invite_free_list(profile->group_chat_invites);
    twc_message_queue_free_profile(profile);
    free(profile->name);
    free(profile);

    // remove from list
    twc_list_remove_with_data(twc_profiles, profile);
}

/**
 * Free all profiles.
 */
void
twc_profile_free_all()
{
    struct t_twc_profile *profile;
    while ((profile = twc_list_pop(twc_profiles)))
        twc_profile_free(profile);

    free(twc_profiles);
}

