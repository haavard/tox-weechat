#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

#include <weechat/weechat-plugin.h>

#include "tox-weechat.h"
#include "tox-weechat-identities.h"

#include "tox-weechat-config.h"

struct t_config_file *tox_weechat_config_file = NULL;
struct t_config_section *tox_weechat_config_section_identity = NULL;

char *tox_weechat_identity_option_names[TOX_WEECHAT_IDENTITY_NUM_OPTIONS] =
{
    "save_file",
    "autoconnect",
    "max_friend_requests",
};

char *tox_weechat_identity_option_defaults[TOX_WEECHAT_IDENTITY_NUM_OPTIONS] =
{
    "%h/tox/%n",
    "off",
    "100",
};

int
tox_weechat_config_identity_option_search(const char *option_name)
{
    for (int i = 0; i < TOX_WEECHAT_IDENTITY_NUM_OPTIONS; ++i)
    {
        if (strcmp(tox_weechat_identity_option_names[i], option_name) == 0)
            return i;
    }

    return -1;
}

int
tox_weechat_config_identity_read_callback(void *data,
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
            char *identity_name = weechat_strndup(option_name,
                                                  dot_pos-option_name);
            char *option_name = dot_pos + 1;
            if (identity_name)
            {
                int option_index = tox_weechat_config_identity_option_search(option_name);
                if (option_index >= 0)
                {
                    struct t_tox_weechat_identity *identity =
                        tox_weechat_identity_name_search(identity_name);

                    if (!identity)
                        identity = tox_weechat_identity_new(identity_name);

                    if (identity)
                    {
                        rc = weechat_config_option_set(identity->options[option_index],
                                                       value, 1);
                    }
                    else
                    {
                        weechat_printf(NULL,
                                       "%s%s: error creating identity \"%s\"",
                                       weechat_prefix("error"),
                                       weechat_plugin->name,
                                       identity_name);
                    }
                }

                free(identity_name);
            }
        }
    }

    if (rc == WEECHAT_CONFIG_OPTION_SET_ERROR)
    {
        weechat_printf(NULL,
                       "%s%s: error creating identity option \"%s\"",
                       weechat_prefix("error"),
                       weechat_plugin->name,
                       option_name);
    }

    return rc;
}

int
tox_weechat_config_identity_write_callback(void *data,
                                           struct t_config_file *config_file,
                                           const char *section_name)
{
    if (!weechat_config_write_line (config_file, section_name, NULL))
        return WEECHAT_CONFIG_WRITE_ERROR;

    for (struct t_tox_weechat_identity *identity = tox_weechat_identities;
         identity;
         identity = identity->next_identity)
    {
        for (int i = 0; i < TOX_WEECHAT_IDENTITY_NUM_OPTIONS; ++i)
        {
            if (!weechat_config_write_option(tox_weechat_config_file,
                                             identity->options[i]))
            {
                return WEECHAT_CONFIG_WRITE_ERROR;
            }
        }
    }

    return WEECHAT_CONFIG_WRITE_OK;
}

int
tox_weechat_config_identity_check_value_callback(void *data,
                                                 struct t_config_option *option,
                                                 const char *value)
{
    return 1; // ok, 0 not ok
}

void
tox_weechat_config_identity_change_callback(void *data,
                                            struct t_config_option *option)
{
}

void
tox_weechat_config_init()
{

    tox_weechat_config_file = weechat_config_new("tox", NULL, NULL);

    tox_weechat_config_section_identity =
        weechat_config_new_section(tox_weechat_config_file, "identity",
                                   0, 0,
                                   tox_weechat_config_identity_read_callback, NULL,
                                   tox_weechat_config_identity_write_callback, NULL,
                                   NULL, NULL,
                                   NULL, NULL,
                                   NULL, NULL);
}

struct t_config_option *
tox_weechat_config_init_option(int option_index, const char *option_name)
{
    switch (option_index)
    {
        case TOX_WEECHAT_IDENTITY_OPTION_AUTOCONNECT:
            return weechat_config_new_option(
                tox_weechat_config_file, tox_weechat_config_section_identity,
                option_name, "boolean",
                "automatically connect to the Tox network when WeeChat starts",
                NULL, 0, 0,
                tox_weechat_identity_option_defaults[option_index],
                NULL,
                0,
                tox_weechat_config_identity_check_value_callback, NULL,
                tox_weechat_config_identity_change_callback, NULL,
                NULL, NULL);
        case TOX_WEECHAT_IDENTITY_OPTION_MAX_FRIEND_REQUESTS:
            return weechat_config_new_option(
                tox_weechat_config_file, tox_weechat_config_section_identity,
                option_name, "integer",
                "maximum amount of friend requests to retain before dropping "
                "new ones",
                NULL, 0, INT_MAX,
                tox_weechat_identity_option_defaults[option_index],
                NULL,
                0,
                tox_weechat_config_identity_check_value_callback, NULL,
                tox_weechat_config_identity_change_callback, NULL,
                NULL, NULL);
        case TOX_WEECHAT_IDENTITY_OPTION_SAVEFILE:
            return weechat_config_new_option(
                tox_weechat_config_file, tox_weechat_config_section_identity,
                option_name, "string",
                "path to Tox data file (\"%h\" will be replaced by WeeChat "
                "home, \"%n\" by the identity name); will be created if it does "
                "not exist.",
                NULL, 0, 0,
                tox_weechat_identity_option_defaults[option_index],
                NULL,
                0,
                tox_weechat_config_identity_check_value_callback, NULL,
                tox_weechat_config_identity_change_callback, NULL,
                NULL, NULL);
        default:
            return NULL;
    }
}

void
tox_weechat_config_init_identity(struct t_tox_weechat_identity *identity)
{
    for (int i = 0; i < TOX_WEECHAT_IDENTITY_NUM_OPTIONS; ++i)
    {
        // length: name + . + option + \0
        size_t length = strlen(identity->name) + 1
                        + strlen(tox_weechat_identity_option_names[i]) + 1;

        char *option_name = malloc(sizeof(*option_name) * length);
        if (option_name)
        {
            snprintf(option_name, length, "%s.%s",
                     identity->name,
                     tox_weechat_identity_option_names[i]);

            identity->options[i] = tox_weechat_config_init_option(i, option_name);
            free (option_name);
        }
    }
}

int
tox_weechat_config_read()
{
    return weechat_config_read(tox_weechat_config_file);
}

int
tox_weechat_config_write()
{
    return weechat_config_write(tox_weechat_config_file);
}
