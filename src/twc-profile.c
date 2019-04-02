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

#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <tox/tox.h>
#include <weechat/weechat-plugin.h>
#ifdef TOXENCRYPTSAVE_ENABLED
#include <tox/toxencryptsave.h>
#endif /* TOXENCRYPTSAVE_ENABLED */

#include "twc-bootstrap.h"
#include "twc-chat.h"
#include "twc-config.h"
#include "twc-friend-request.h"
#include "twc-group-invite.h"
#include "twc-list.h"
#include "twc-message-queue.h"
#include "twc-tox-callbacks.h"
#include "twc-utils.h"
#include "twc.h"

#include "twc-profile.h"

struct t_twc_list *twc_profiles = NULL;
struct t_config_option *twc_config_profile_default[TWC_PROFILE_NUM_OPTIONS];

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
    const char *weechat_dir = weechat_info_get("weechat_dir", NULL);
    const char *base_path =
        TWC_PROFILE_OPTION_STRING(profile, TWC_PROFILE_OPTION_SAVEFILE);
    char *home_expanded = weechat_string_replace(base_path, "%h", weechat_dir);
    char *full_path =
        weechat_string_replace(home_expanded, "%p", profile->name);
    free(home_expanded);

    return full_path;
}

/**
 * Save a profile's Tox data to disk.
 *
 * Returns 0 on success, -1 on failure.
 */
int
twc_profile_save_data_file(struct t_twc_profile *profile)
{
    if (!(profile->tox))
        return -1;

    char *full_path = twc_profile_expanded_data_path(profile);

    /* create containing folder if it doesn't exist */
    char *rightmost_slash = strrchr(full_path, '/');
    char *dir_path = weechat_strndup(full_path, rightmost_slash - full_path);
    weechat_mkdir_parents(dir_path, 0755);
    free(dir_path);

    /* save Tox data to a buffer */
    size_t size = tox_get_savedata_size(profile->tox);
    uint8_t data[size];
    uint8_t *d = data;
    tox_get_savedata(profile->tox, data);

#ifdef TOXENCRYPTSAVE_ENABLED
    uint8_t enc_data[size + TOX_PASS_ENCRYPTION_EXTRA_LENGTH];
    const char *pw =
        weechat_config_string(profile->options[TWC_PROFILE_OPTION_PASSPHRASE]);

    if (pw)
    {
        pw = weechat_string_eval_expression(pw, NULL, NULL, NULL);
        if (!tox_pass_encrypt(data, size, (uint8_t *)pw, strlen(pw), enc_data,
                              NULL))
        {
            weechat_printf(profile->buffer, "error encrypting data");
            return -1;
        }
        d = enc_data;
        size += TOX_PASS_ENCRYPTION_EXTRA_LENGTH;
    }
#endif /* TOXENCRYPTSAVE_ENABLED */

    /* save buffer to a file */
    FILE *file = fopen(full_path, "w");
    if (file)
    {
        size_t saved_size = fwrite(d, 1, size, file);
        fclose(file);

        return saved_size == size;
    }

    return -1;
}

/**
 * Callback when a profile's main buffer is closed. Unloads the profile.
 */
int
twc_profile_buffer_close_callback(const void *pointer, void *data,
                                  struct t_gui_buffer *buffer)
{
    struct t_twc_profile *profile = (void *)pointer;

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

    /* add to profile list */
    twc_list_item_new_data_add(twc_profiles, profile);

    /* set up internal vars */
    profile->tox = NULL;
    profile->buffer = NULL;
    profile->tox_do_timer = NULL;
    profile->tox_online = false;

    profile->chats = twc_list_new();
    profile->ignores = weechat_list_new();
    profile->friend_requests = twc_list_new();
    profile->group_chat_invites = twc_list_new();
    profile->message_queues = weechat_hashtable_new(
        32, WEECHAT_HASHTABLE_INTEGER, WEECHAT_HASHTABLE_POINTER, NULL, NULL);
    profile->tfer = twc_tfer_new();

    /* set up config */
    twc_config_init_profile(profile);
    twc_tfer_update_downloading_path(profile);

    return profile;
}

/**
 * Load Tox options from WeeChat configuration files into a Tox_Options struct.
 */
void
twc_profile_set_options(struct Tox_Options *options,
                        struct t_twc_profile *profile)
{
    tox_options_default(options);

    const char *proxy_host =
        TWC_PROFILE_OPTION_STRING(profile, TWC_PROFILE_OPTION_PROXY_ADDRESS);
    if (proxy_host)
        options->proxy_host = proxy_host;

    switch (TWC_PROFILE_OPTION_INTEGER(profile, TWC_PROFILE_OPTION_PROXY_TYPE))
    {
        case TWC_PROXY_NONE:
            options->proxy_type = TOX_PROXY_TYPE_NONE;
            break;
        case TWC_PROXY_SOCKS5:
            options->proxy_type = TOX_PROXY_TYPE_SOCKS5;
            break;
        case TWC_PROXY_HTTP:
            options->proxy_type = TOX_PROXY_TYPE_HTTP;
            break;
    }

    options->proxy_port =
        TWC_PROFILE_OPTION_INTEGER(profile, TWC_PROFILE_OPTION_PROXY_PORT);
    options->udp_enabled =
        TWC_PROFILE_OPTION_BOOLEAN(profile, TWC_PROFILE_OPTION_UDP);
    options->ipv6_enabled =
        TWC_PROFILE_OPTION_BOOLEAN(profile, TWC_PROFILE_OPTION_IPV6);

#ifndef NDEBUG
    options->log_callback = twc_tox_log_callback;
    options->log_user_data = profile;
#endif /* !NDEBUG */
}

void
twc_tox_new_print_error(struct t_twc_profile *profile,
                        struct Tox_Options *options, TOX_ERR_NEW error)
{
    switch (error)
    {
        case TOX_ERR_NEW_MALLOC:
            weechat_printf(profile->buffer,
                           "%scould not load Tox (malloc error)",
                           weechat_prefix("error"));
            break;
        case TOX_ERR_NEW_PORT_ALLOC:
            weechat_printf(profile->buffer,
                           "%scould not load Tox (failed to allocate a port)",
                           weechat_prefix("error"));
            break;
        case TOX_ERR_NEW_PROXY_BAD_TYPE:
            weechat_printf(
                profile->buffer,
                "%scould not load Tox (internal error; bad proxy type)",
                weechat_prefix("error"));
            break;
        case TOX_ERR_NEW_PROXY_BAD_HOST:
            weechat_printf(profile->buffer,
                           "%scould not load Tox (invalid proxy host: \"%s\")",
                           weechat_prefix("error"), options->proxy_host);
            break;
        case TOX_ERR_NEW_PROXY_BAD_PORT:
            weechat_printf(
                profile->buffer,
                "%scould not load Tox (invalid proxy port: \"%" PRIu16 "\")",
                weechat_prefix("error"), options->proxy_port);
            break;
        case TOX_ERR_NEW_PROXY_NOT_FOUND:
            weechat_printf(
                profile->buffer,
                "%scould not load Tox (proxy host not found: \"%s\")",
                weechat_prefix("error"), options->proxy_host);
            break;
        case TOX_ERR_NEW_LOAD_ENCRYPTED:
            weechat_printf(profile->buffer,
                           "%scould not load Tox (encrypted "
                           "data files are not yet supported)",
                           weechat_prefix("error"));
            break;
        case TOX_ERR_NEW_LOAD_BAD_FORMAT:
            weechat_printf(profile->buffer,
                           "%scould not load Tox (invalid data file, some data "
                           "may have been loaded; use -force to try using it)",
                           weechat_prefix("error"));
            break;
        default:
            weechat_printf(profile->buffer,
                           "%scould not load Tox (unknown error %d)",
                           weechat_prefix("error"), error);
            break;
    }
}

/**
 * Load a profile's Tox object, creating a new one if it can't be loaded from
 * disk, and bootstraps the Tox DHT.
 */
enum t_twc_rc
twc_profile_load(struct t_twc_profile *profile)
{
    if (profile->tox)
        return TWC_RC_ERROR;

    if (!(profile->buffer))
    {
        /* create main buffer */
        profile->buffer = weechat_buffer_new(profile->name, NULL, NULL, NULL,
                                             twc_profile_buffer_close_callback,
                                             profile, NULL);
        if (!(profile->buffer))
            return TWC_RC_ERROR;

        /* disable logging for buffer if option is off */
        bool logging =
            TWC_PROFILE_OPTION_BOOLEAN(profile, TWC_PROFILE_OPTION_LOGGING);
        twc_set_buffer_logging(profile->buffer, logging);

        profile->nicklist_group =
            weechat_nicklist_add_group(profile->buffer, NULL, NULL, NULL, true);
        weechat_buffer_set(profile->buffer, "nicklist", "1");
    }

    weechat_printf(profile->buffer, "%sprofile %s connecting",
                   weechat_prefix("network"), profile->name);

    /* create Tox options object */
    struct Tox_Options options;

    twc_profile_set_options(&options, profile);

    /* print a proxy message */
    if (options.proxy_type != TOX_PROXY_TYPE_NONE)
    {
        weechat_printf(profile->buffer, "%susing %s proxy %s:%" PRIu16,
                       weechat_prefix("network"),
                       options.proxy_type == TOX_PROXY_TYPE_HTTP
                           ? "HTTP"
                           : TOX_PROXY_TYPE_SOCKS5 ? "SOCKS5" : NULL,
                       options.proxy_host, options.proxy_port);

        if (options.udp_enabled)
            weechat_printf(profile->buffer,
                           "%s%swarning:%s Tox is configured to use a proxy, "
                           "but UDP is not disabled. Your IP address may not "
                           "be hidden.",
                           weechat_prefix("error"), weechat_color("lightred"),
                           weechat_color("reset"), options.proxy_host,
                           options.proxy_port);
    }

    /* try loading data file */
    char *path = twc_profile_expanded_data_path(profile);
    FILE *file = NULL;
    size_t data_size;

    if (!(file = fopen(path, "r")))
    {
        data_size = 0;
    }
    else
    {
        fseek(file, 0, SEEK_END);
        data_size = ftell(file);
    }

    uint8_t data[data_size];

    if (file)
    {
        rewind(file);
        if ((data_size != fread(&data, 1, data_size, file)))
        {
            fclose(file);
            weechat_printf(profile->buffer,
                           "%scould not load Tox data file, aborting",
                           weechat_prefix("error"));
            return TWC_RC_ERROR;
        }
        fclose(file);
    }

    options.savedata_data = data;
    options.savedata_length = data_size;

#if TOXENCRYPTSAVE_ENABLED
    uint8_t dec_data[data_size];

    if (data_size && tox_is_data_encrypted(data))
    {
        const char *pw = weechat_config_string(
            profile->options[TWC_PROFILE_OPTION_PASSPHRASE]);

        if (pw)
        {
            /* evaluate password option and duplicate as tox_*_decrypt wipes
             * it */
            pw = weechat_string_eval_expression(pw, NULL, NULL, NULL);
            if (!tox_pass_decrypt(data, data_size, (uint8_t *)pw, strlen(pw),
                                  dec_data, NULL))
            {
                weechat_printf(profile->buffer,
                               "%scould not decrypt Tox data file, aborting",
                               weechat_prefix("error"));
                return TWC_RC_ERROR;
            }
            options.savedata_data = dec_data;
            options.savedata_length -= TOX_PASS_ENCRYPTION_EXTRA_LENGTH;
        }
        else
        {
            weechat_printf(profile->buffer,
                           "%scould not decrypt Tox data file (no passphrase "
                           "specified)",
                           weechat_prefix("error"));
            return TWC_RC_ERROR;
        }
    }
#endif /* TOXENCRYPTSAVE_ENABLED */

    options.savedata_type =
        (data_size == 0) ? TOX_SAVEDATA_TYPE_NONE : TOX_SAVEDATA_TYPE_TOX_SAVE;

    /* create Tox */
    TOX_ERR_NEW rc;
    profile->tox = tox_new(&options, &rc);
    if (rc != TOX_ERR_NEW_OK)
    {
        twc_tox_new_print_error(profile, &options, rc);
        profile->tox = NULL;
        return rc == TOX_ERR_NEW_MALLOC ? TWC_RC_ERROR_MALLOC : TWC_RC_ERROR;
    }

    if (data_size == 0)
    {
        /* no data file loaded, set default name */
        const char *default_name = "Tox-WeeChat User";

        const char *name;
        struct passwd *user_pwd;
        if ((user_pwd = getpwuid(geteuid())))
            name = user_pwd->pw_name;
        else
            name = default_name;

        TOX_ERR_SET_INFO rc;
        tox_self_set_name(profile->tox, (uint8_t *)name, strlen(name), &rc);

        if (rc == TOX_ERR_SET_INFO_TOO_LONG)
            tox_self_set_name(profile->tox, (uint8_t *)default_name,
                              strlen(default_name), NULL);
    }

    /* bootstrap DHT
     * TODO: add count to config */
    int bootstrap_node_count = 5;
    for (int i = 0; i < bootstrap_node_count; ++i)
        twc_bootstrap_random_node(profile->tox);

    /* start tox_iterate loop */
    twc_do_timer_cb(profile, NULL, 0);

    /* register Tox callbacks */
    tox_callback_friend_message(profile->tox, twc_friend_message_callback);
    tox_callback_friend_connection_status(profile->tox,
                                          twc_connection_status_callback);
    tox_callback_friend_name(profile->tox, twc_name_change_callback);
    tox_callback_friend_status(profile->tox, twc_user_status_callback);
    tox_callback_friend_status_message(profile->tox,
                                       twc_status_message_callback);
    tox_callback_friend_request(profile->tox, twc_friend_request_callback);
    tox_callback_conference_invite(profile->tox, twc_group_invite_callback);
    tox_callback_conference_message(profile->tox, twc_group_message_callback);
    tox_callback_conference_peer_list_changed(
        profile->tox, twc_group_peer_list_changed_callback);
    tox_callback_conference_peer_name(profile->tox,
                                      twc_group_peer_name_callback);
    tox_callback_conference_title(profile->tox, twc_group_title_callback);
    tox_callback_file_recv_control(profile->tox,
                                   twc_file_recv_control_callback);
    tox_callback_file_chunk_request(profile->tox,
                                    twc_file_chunk_request_callback);
    tox_callback_file_recv(profile->tox, twc_file_recv_callback);
    tox_callback_file_recv_chunk(profile->tox, twc_file_recv_chunk_callback);

    return TWC_RC_OK;
}

/**
 * Unload a Tox profile. Disconnects from the network, saves data to disk.
 */
void
twc_profile_unload(struct t_twc_profile *profile)
{
    /* check that we're not already disconnected */
    if (!(profile->tox))
        return;

    /* save and kill tox */
    int result = twc_profile_save_data_file(profile);
    tox_kill(profile->tox);
    profile->tox = NULL;

    if (result == -1)
    {
        char *path = twc_profile_expanded_data_path(profile);
        weechat_printf(
            NULL, "%s%s: Could not save Tox data for profile %s to file: %s",
            weechat_prefix("error"), weechat_plugin->name, profile->name, path);
        free(path);
    }

    /* stop Tox timer */
    weechat_unhook(profile->tox_do_timer);

    /* have to refresh and hide bar items even if we were already offline
     * TODO */
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
    twc_list_foreach (twc_profiles, index, item)
    {
        if (TWC_PROFILE_OPTION_BOOLEAN(item->profile,
                                       TWC_PROFILE_OPTION_AUTOLOAD))
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
twc_profile_set_online_status(struct t_twc_profile *profile, bool status)
{
    if (profile->tox_online ^ status)
    {
        profile->tox_online = status;
        twc_profile_refresh_online_status(profile);

        if (profile->tox_online)
        {
            weechat_printf(profile->buffer, "%sprofile %s connected",
                           weechat_prefix("network"), profile->name);
        }
        else
        {
            weechat_printf(profile->buffer, "%sprofile %s disconnected",
                           weechat_prefix("network"), profile->name);
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
    twc_list_foreach (twc_profiles, index, item)
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
    twc_list_foreach (twc_profiles, profile_index, profile_item)
    {
        if (profile_item->profile->buffer == buffer)
            return profile_item->profile;
        if (profile_item->profile->tfer->buffer == buffer)
            return profile_item->profile;

        size_t chat_index;
        struct t_twc_list_item *chat_item;
        twc_list_foreach (profile_item->profile->chats, chat_index, chat_item)
        {
            if (chat_item->chat->buffer == buffer)
                return profile_item->profile;
        }
    }

    return NULL;
}

/**
 * Return the profile associated with a tox instance, if any.
 */

struct t_twc_profile *
twc_profile_search_tox(struct Tox *tox)
{
    size_t profile_index;
    struct t_twc_list_item *profile_item;
    twc_list_foreach (twc_profiles, profile_index, profile_item)
    {
        if (profile_item->profile->tox == tox)
            return profile_item->profile;
    }
    return NULL;
}

/**
 * Enable or disable the WeeChat logger for all buffers for a profile.
 *
 * Return TWC_RC_OK if logging was enabled or disabled for all buffers.
 * Return TWC_RC_ERROR if one or more errors occurred.
 */
enum t_twc_rc
twc_profile_set_logging(struct t_twc_profile *profile, bool logging)
{
    /* track if an error occurs */
    bool error;

    /* if profile buffer is NULL, profile isn't loaded */
    if (!profile->buffer)
        return TWC_RC_OK;

    /* signal profile's main buffer */
    if (WEECHAT_RC_ERROR == twc_set_buffer_logging(profile->buffer, logging))
    {
        error = true;
        weechat_printf(profile->buffer,
                       "%swarning: failed to %s logging in this buffer!",
                       weechat_prefix("error"), logging ? "enable" : "disable");
    }

    /* signal all chat buffers for profile */
    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach (profile->chats, index, item)
    {
        if (WEECHAT_RC_ERROR ==
            twc_set_buffer_logging(item->chat->buffer, logging))
        {
            error = true;
            weechat_printf(item->chat->buffer,
                           "%swarning: failed to %s logging in this buffer!",
                           weechat_prefix("error"),
                           logging ? "enable" : "disable");
        }
    }

    return error ? TWC_RC_ERROR : TWC_RC_OK;
}

/**
 * Delete a profile. Unloads, frees and deletes everything. If delete_data is
 * true, Tox data on disk is also deleted.
 */
void
twc_profile_delete(struct t_twc_profile *profile, bool delete_data)
{
    char *data_path = twc_profile_expanded_data_path(profile);

    for (size_t i = 0; i < TWC_PROFILE_NUM_OPTIONS; ++i)
        weechat_config_option_free(profile->options[i]);

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
    /* unload if needed */
    twc_profile_unload(profile);

    /* close buffer */
    if (profile->buffer)
    {
        weechat_buffer_set_pointer(profile->buffer, "close_callback", NULL);
        weechat_buffer_close(profile->buffer);
    }
    /* close tfer's buffer */
    if (profile->tfer->buffer)
    {
        weechat_buffer_set_pointer(profile->tfer->buffer, "close_callback",
                                   NULL);
        weechat_buffer_close(profile->tfer->buffer);
    }

    /* free things */
    twc_chat_free_list(profile->chats);
    weechat_list_remove_all(profile->ignores);
    weechat_list_free(profile->ignores);
    twc_friend_request_free_list(profile->friend_requests);
    twc_group_chat_invite_free_list(profile->group_chat_invites);
    twc_tfer_free(profile->tfer);
    twc_message_queue_free_profile(profile);
    free(profile->name);
    free(profile);

    /* remove from list */
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
