/*
 * Copyright (c) 2017 Håvard Pettersson <mail@haavard.me>
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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <weechat/weechat-plugin.h>

#include "twc-list.h"
#include "twc-profile.h"
#include "twc.h"

#include "twc-config.h"

struct t_config_file *twc_config_file = NULL;
struct t_config_section *twc_config_section_look = NULL;
struct t_config_section *twc_config_section_profile = NULL;
struct t_config_section *twc_config_section_profile_default = NULL;

struct t_config_option *twc_config_friend_request_message;
struct t_config_option *twc_config_short_id_size;

char *twc_profile_option_names[TWC_PROFILE_NUM_OPTIONS] = {
    "save_file", "autoload", "autojoin", "autojoin_delay",
    "max_friend_requests", "proxy_address", "proxy_port", "proxy_type",
    "udp", "ipv6", "passphrase", "logging",
};

/**
 * Get the index of a profile option name.
 */
int
twc_config_profile_option_search(const char *option_name)
{
    for (int i = 0; i < TWC_PROFILE_NUM_OPTIONS; ++i)
    {
        if (strcmp(twc_profile_option_names[i], option_name) == 0)
            return i;
    }

    return -1;
}

/**
 * Called when a profile option is read.
 */
int
twc_config_profile_read_callback(const void *pointer, void *data,
                                 struct t_config_file *config_file,
                                 struct t_config_section *section,
                                 const char *option_name, const char *value)
{
    int rc = WEECHAT_CONFIG_OPTION_SET_ERROR;

    if (option_name)
    {
        char *dot_pos = strrchr(option_name, '.');
        if (dot_pos)
        {
            char *profile_name =
                weechat_strndup(option_name, dot_pos - option_name);
            char *option_name = dot_pos + 1;
            if (profile_name)
            {
                int option_index =
                    twc_config_profile_option_search(option_name);
                if (option_index >= 0)
                {
                    struct t_twc_profile *profile =
                        twc_profile_search_name(profile_name);

                    if (!profile)
                        profile = twc_profile_new(profile_name);

                    if (profile)
                    {
                        rc = weechat_config_option_set(
                            profile->options[option_index], value, 1);
                    }
                    else
                    {
                        weechat_printf(NULL,
                                       "%s%s: error creating profile \"%s\"",
                                       weechat_prefix("error"),
                                       weechat_plugin->name, profile_name);
                    }
                }

                free(profile_name);
            }
        }
    }

    if (rc == WEECHAT_CONFIG_OPTION_SET_ERROR)
    {
        weechat_printf(NULL, "%s%s: error creating profile option \"%s\"",
                       weechat_prefix("error"), weechat_plugin->name,
                       option_name);
    }

    return rc;
}

/**
 * Callback for checking an option value being set.
 */
int
twc_config_check_value_callback(const void *pointer, void *data,
                                struct t_config_option *option,
                                const char *value)
{
    int int_value = atoi(value);

    /* must be multiple of two */
    if (option == twc_config_short_id_size && int_value % 2)
        return 0;

    return 1;
}

/**
 * Callback for checking an option value being set for a profile.
 */
int
twc_config_profile_check_value_callback(const void *pointer, void *data,
                                        struct t_config_option *option,
                                        const char *value)
{
    enum t_twc_profile_option option_index = *(int *)data;

    switch (option_index)
    {
        case TWC_PROFILE_OPTION_PROXY_ADDRESS:
            return !value || strlen(value) < 256;
        default:
            return 1;
    }
}

/**
 * Callback for option being changed for a profile.
 */
void
twc_config_profile_change_callback(const void *pointer, void *data,
                                   struct t_config_option *option)
{
    struct t_twc_profile *profile = (void *)pointer;
    enum t_twc_profile_option option_index = *(int *)data;

    switch (option_index)
    {
        case TWC_PROFILE_OPTION_LOGGING:
            if (profile)
            {
                /* toggle logging for a specific profile */
                bool logging_enabled = weechat_config_boolean(option);
                twc_profile_set_logging(profile, logging_enabled);
            }
            else
            {
                /* option changed for default profile, update all profiles */
                size_t index;
                struct t_twc_list_item *item;
                twc_list_foreach (twc_profiles, index, item)
                {
                    bool logging_enabled = TWC_PROFILE_OPTION_BOOLEAN(
                        item->profile, TWC_PROFILE_OPTION_LOGGING);
                    twc_profile_set_logging(item->profile, logging_enabled);
                }
            }

        default:
            break;
    }
}

/**
 * Create a new option for a profile. Returns NULL if an error occurs.
 */
struct t_config_option *
twc_config_init_option(struct t_twc_profile *profile,
                       struct t_config_section *section, int option_index,
                       const char *option_name, bool is_default_profile)
{
    char *type;
    char *description;
    char *string_values = NULL;
    int min = 0, max = 0;
    char *value;
    char *default_value = NULL;
    bool null_allowed = false;

    switch (option_index)
    {
        case TWC_PROFILE_OPTION_AUTOLOAD:
            type = "boolean";
            description = "automatically load profile and connect to the Tox "
                          "network when WeeChat starts";
            default_value = "off";
            break;
        case TWC_PROFILE_OPTION_AUTOJOIN:
            type = "boolean";
            description = "automatically join all groups you are invited in "
                          "by your friends";
            default_value = "off";
            break;
        case TWC_PROFILE_OPTION_AUTOJOIN_DELAY:
            type = "integer";
            description = "delay befor do autojoin (in ms) this required to "
                          "tox from entering incorrect state and stop processing "
                          "group events";
            min = 0;
            max = INT_MAX;
            default_value = "5000";
            break;
        case TWC_PROFILE_OPTION_IPV6:
            type = "boolean";
            description = "use IPv6 as well as IPv4 to connect to the Tox "
                          "network";
            default_value = "on";
            break;
        case TWC_PROFILE_OPTION_LOGGING:
            type = "boolean";
            description = "log chat buffers to disk";
            default_value = "off";
            break;
        case TWC_PROFILE_OPTION_MAX_FRIEND_REQUESTS:
            type = "integer";
            description = "maximum amount of friend requests to retain before "
                          "ignoring new ones";
            min = 0;
            max = INT_MAX;
            default_value = "100";
            break;
        case TWC_PROFILE_OPTION_PASSPHRASE:
            type = "string";
            description = "passphrase for encrypted profile";
            null_allowed = true;
            break;
        case TWC_PROFILE_OPTION_PROXY_ADDRESS:
            type = "string";
            description = "proxy address";
            null_allowed = true;
            break;
        case TWC_PROFILE_OPTION_PROXY_PORT:
            type = "integer";
            description = "proxy port";
            min = 0;
            max = UINT16_MAX;
            null_allowed = true;
            break;
        case TWC_PROFILE_OPTION_PROXY_TYPE:
            type = "integer";
            description = "proxy type; requires profile reload to take effect";
            string_values = "none|socks5|http";
            min = 0;
            max = 0;
            default_value = "none";
            break;
        case TWC_PROFILE_OPTION_SAVEFILE:
            type = "string";
            description = "path to Tox data file (\"%h\" will be replaced by "
                          "WeeChat home folder and \"%p\" by profile name";
            default_value = "%h/tox/%p";
            break;
        case TWC_PROFILE_OPTION_UDP:
            type = "boolean";
            description = "use UDP when communicating with the Tox network";
            default_value = "on";
            break;
        default:
            return NULL;
    }

    null_allowed = null_allowed || !is_default_profile;
    value = is_default_profile ? default_value : NULL;

    /* store option index as data for WeeChat callbacks */
    int *index_check_pointer = malloc(sizeof(int));
    int *index_change_pointer = malloc(sizeof(int));
    if (!index_check_pointer || !index_change_pointer)
    {
        free(index_check_pointer);
        free(index_change_pointer);
        return NULL;
    }
    *index_check_pointer = option_index;
    *index_change_pointer = option_index;

    return weechat_config_new_option(
        twc_config_file, section, option_name, type, description, string_values,
        min, max, default_value, value, null_allowed,
        twc_config_profile_check_value_callback, profile, index_check_pointer,
        twc_config_profile_change_callback, profile, index_change_pointer, NULL,
        NULL, NULL);
}

/**
 * Initialize Tox-WeeChat config. Creates file and section objects.
 */
void
twc_config_init()
{
    twc_config_file = weechat_config_new("tox", NULL, NULL, NULL);

    twc_config_section_profile = weechat_config_new_section(
        twc_config_file, "profile", 0, 0, twc_config_profile_read_callback,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL);

    twc_config_section_profile_default = weechat_config_new_section(
        twc_config_file, "profile_default", 0, 0, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    for (int i = 0; i < TWC_PROFILE_NUM_OPTIONS; ++i)
    {
        twc_config_profile_default[i] =
            twc_config_init_option(NULL, twc_config_section_profile_default, i,
                                   twc_profile_option_names[i], true);
    }

    twc_config_section_look = weechat_config_new_section(
        twc_config_file, "look", 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    twc_config_friend_request_message = weechat_config_new_option(
        twc_config_file, twc_config_section_look, "friend_request_message",
        "string",
        "message sent with friend requests if no other message is specified",
        NULL, 0, 0, "Hi! Please add me on Tox!", NULL, 0,
        twc_config_check_value_callback, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL);
    twc_config_short_id_size = weechat_config_new_option(
        twc_config_file, twc_config_section_look, "short_id_size", "integer",
        "length of Tox IDs shown in short format; must be a multiple of two",
        NULL, 2, TOX_PUBLIC_KEY_SIZE * 2, "8", NULL, 0,
        twc_config_check_value_callback, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL);
}

/**
 * Initialize options for a given profile.
 */
void
twc_config_init_profile(struct t_twc_profile *profile)
{
    for (int i = 0; i < TWC_PROFILE_NUM_OPTIONS; ++i)
    {
        /* length: name + . + option + \0 */
        size_t length =
            strlen(profile->name) + 1 + strlen(twc_profile_option_names[i]) + 1;

        char *option_name = malloc(sizeof(*option_name) * length);
        if (option_name)
        {
            snprintf(option_name, length, "%s.%s", profile->name,
                     twc_profile_option_names[i]);

            profile->options[i] = twc_config_init_option(
                profile, twc_config_section_profile, i, option_name, false);
            free(option_name);
        }
    }
}

/**
 * Read config data from file, creating profile objects for stored profiles.
 */
int
twc_config_read()
{
    return weechat_config_read(twc_config_file);
}

/**
 * Write config data to disk.
 */
int
twc_config_write()
{
    return weechat_config_write(twc_config_file);
}
