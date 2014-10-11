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

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

#include <weechat/weechat-plugin.h>

#include "twc.h"
#include "twc-list.h"
#include "twc-profile.h"

#include "twc-config.h"

struct t_config_file *twc_config_file = NULL;
struct t_config_section *twc_config_section_profile = NULL;
struct t_config_section *twc_config_section_profile_default = NULL;

char *twc_profile_option_names[TWC_PROFILE_NUM_OPTIONS] =
{
    "save_file",
    "autoload",
    "max_friend_requests",
    "proxy_address",
    "proxy_port",
    "proxy",
    "udp",
    "ipv6",
};

char *twc_profile_option_defaults[TWC_PROFILE_NUM_OPTIONS] =
{
    "%h/tox/%p",
    "off",
    "100",
    NULL,
    NULL,
    "off",
    "on",
    "on",
};

bool twc_profile_option_null_allowed[TWC_PROFILE_NUM_OPTIONS] =
{
    false,
    false,
    false,
    true, // we allow proxy information to be null
    true, // -------------------------------------
    false,
    false,
    false,
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
twc_config_profile_read_callback(void *data,
                                 struct t_config_file *config_file,
                                 struct t_config_section *section,
                                 const char *option_name,
                                 const char *value)
{
    int rc = WEECHAT_CONFIG_OPTION_SET_ERROR;

    if (option_name)
    {
        char *dot_pos = strrchr(option_name, '.');
        if (dot_pos)
        {
            char *profile_name = weechat_strndup(option_name,
                                                 dot_pos-option_name);
            char *option_name = dot_pos + 1;
            if (profile_name)
            {
                int option_index = twc_config_profile_option_search(option_name);
                if (option_index >= 0)
                {
                    struct t_twc_profile *profile =
                        twc_profile_search_name(profile_name);

                    if (!profile)
                        profile = twc_profile_new(profile_name);

                    if (profile)
                    {
                        rc = weechat_config_option_set(profile->options[option_index],
                                                       value, 1);
                    }
                    else
                    {
                        weechat_printf(NULL,
                                       "%s%s: error creating profile \"%s\"",
                                       weechat_prefix("error"),
                                       weechat_plugin->name,
                                       profile_name);
                    }
                }

                free(profile_name);
            }
        }
    }

    if (rc == WEECHAT_CONFIG_OPTION_SET_ERROR)
    {
        weechat_printf(NULL,
                       "%s%s: error creating profile option \"%s\"",
                       weechat_prefix("error"), weechat_plugin->name,
                       option_name);
    }

    return rc;
}

/**
 * Called when profile options should be written.
 */
int
twc_config_profile_write_callback(void *data,
                                  struct t_config_file *config_file,
                                  const char *section_name)
{
    if (!weechat_config_write_line (config_file, section_name, NULL))
        return WEECHAT_CONFIG_WRITE_ERROR;

    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach(twc_profiles, index, item)
    {
        for (int i = 0; i < TWC_PROFILE_NUM_OPTIONS; ++i)
        {
            if (!weechat_config_write_option(twc_config_file,
                                             item->profile->options[i]))
            {
                return WEECHAT_CONFIG_WRITE_ERROR;
            }
        }
    }

    return WEECHAT_CONFIG_WRITE_OK;
}

/**
 * Callback for checking an option value being set for a profile.
 */
int
twc_config_profile_check_value_callback(void *data,
                                        struct t_config_option *option,
                                        const char *value)
{
    enum t_twc_profile_option option_index = (intptr_t)data;

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
twc_config_profile_change_callback(void *data,
                                   struct t_config_option *option)
{
}

/**
 * Create a new option for a profile.
 */
struct t_config_option *
twc_config_init_option(struct t_config_section *section,
                       int option_index, const char *option_name,
                       bool is_default_profile)
{
    char *type;
    char *description;
    char *string_values = NULL;
    int min = 0, max = 0;

    switch (option_index)
    {
        case TWC_PROFILE_OPTION_AUTOLOAD:
            type = "boolean";
            description = "automatically load profile and connect to the Tox "
                          "network when WeeChat starts";
            break;
        case TWC_PROFILE_OPTION_IPV6:
            type = "boolean";
            description = "use IPv6 as well as IPv4 to connect to the Tox "
                          "network";
            break;
        case TWC_PROFILE_OPTION_MAX_FRIEND_REQUESTS:
            type = "integer";
            description = "maximum amount of friend requests to retain before "
                          "ignoring new ones";
            min = 0; max = INT_MAX;
            break;
        case TWC_PROFILE_OPTION_PROXY_ADDRESS:
            type = "string";
            description = "proxy address";
            break;
        case TWC_PROFILE_OPTION_PROXY_ENABLED:
            type = "boolean";
            description = "use a proxy for communicating with the Tox "
                          "network; requries profile reload to take effect";
            break;
        case TWC_PROFILE_OPTION_PROXY_PORT:
            type = "integer";
            description = "proxy port";
            min = 0; max = UINT16_MAX;
            break;
        case TWC_PROFILE_OPTION_SAVEFILE:
            type = "string";
            description = "path to Tox data file (\"%h\" will be replaced by "
                          "WeeChat home folder and \"%p\" by profile name";
            break;
        case TWC_PROFILE_OPTION_UDP:
            type = "boolean";
            description = "use UDP when communicating with the Tox network";
            break;
        default:
            return NULL;
    }

    char *default_value = twc_profile_option_defaults[option_index];
    char *value = is_default_profile ? default_value : NULL;
    bool null_allowed = !is_default_profile
                        || twc_profile_option_null_allowed[option_index];

    return weechat_config_new_option(
        twc_config_file, section,
        option_name, type, description, string_values, min, max,
        default_value, value, null_allowed,
        twc_config_profile_check_value_callback, (void *)(intptr_t)option_index,
        twc_config_profile_change_callback, (void *)(intptr_t)option_index,
        NULL, NULL);
}

/**
 * Initialize Tox-WeeChat config. Creates file and section objects.
 */
void
twc_config_init()
{
    twc_config_file = weechat_config_new("tox", NULL, NULL);

    twc_config_section_profile =
        weechat_config_new_section(twc_config_file, "profile",
                                   0, 0,
                                   twc_config_profile_read_callback, NULL,
                                   twc_config_profile_write_callback, NULL,
                                   NULL, NULL,
                                   NULL, NULL,
                                   NULL, NULL);

    twc_config_section_profile_default =
        weechat_config_new_section(twc_config_file, "profile_default",
                                   0, 0,
                                   NULL, NULL,
                                   NULL, NULL,
                                   NULL, NULL,
                                   NULL, NULL,
                                   NULL, NULL);

    for (int i = 0; i < TWC_PROFILE_NUM_OPTIONS; ++i)
    {
        twc_config_profile_default[i] =
            twc_config_init_option(twc_config_section_profile_default,
                                   i, twc_profile_option_names[i], true);
    }
}

/**
 * Initialize options for a given profile.
 */
void
twc_config_init_profile(struct t_twc_profile *profile)
{
    for (int i = 0; i < TWC_PROFILE_NUM_OPTIONS; ++i)
    {
        // length: name + . + option + \0
        size_t length = strlen(profile->name) + 1
                        + strlen(twc_profile_option_names[i]) + 1;

        char *option_name = malloc(sizeof(*option_name) * length);
        if (option_name)
        {
            snprintf(option_name, length, "%s.%s",
                     profile->name,
                     twc_profile_option_names[i]);

            profile->options[i] = twc_config_init_option(twc_config_section_profile,
                                                         i, option_name, false);
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

