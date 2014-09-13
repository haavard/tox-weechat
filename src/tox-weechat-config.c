#include <weechat/weechat-plugin.h>

#include "tox-weechat.h"

struct t_config_file *tox_weechat_config_file = NULL;
struct t_config_section *tox_weechat_config_section_identity = NULL;

int
tox_weechat_config_reload_callback(void *data,
                                   struct t_config_file *config_file)
{
    return WEECHAT_RC_OK;
}

int
tox_weechat_config_identity_read_callback(void *data,
                                          struct t_config_file *config_file,
                                          struct t_config_section *section,
                                          const char *option_name,
                                          const char *value)
{
    return WEECHAT_CONFIG_OPTION_SET_OK_CHANGED;
    /* return WEECHAT_CONFIG_OPTION_SET_OK_SAME_VALUE; */
    /* return WEECHAT_CONFIG_OPTION_SET_OPTION_NOT_FOUND; */
    /* return WEECHAT_CONFIG_OPTION_SET_ERROR; */
}

int
tox_weechat_config_identity_write_callback(void *data,
                                           struct t_config_file *config_file,
                                           const char *section_name)
{
    return WEECHAT_CONFIG_WRITE_OK;
    /* return WEECHAT_CONFIG_WRITE_ERROR; */
}

int
tox_weechat_config_identity_write_default_callback(void *data,
                                                   struct t_config_file *config_file,
                                                   const char *section_name)
{
    return WEECHAT_CONFIG_WRITE_OK;
    /* return WEECHAT_CONFIG_WRITE_ERROR; */
}

int
tox_weechat_config_server_name_check_callback(void *data,
                                              struct t_config_option *option,
                                              const char *value)
{
    return 1;
}

void
tox_weechat_config_server_name_change_callback(void *data,
                                               struct t_config_option *option)
{

}

void
tox_weechat_config_init()
{

    tox_weechat_config_file = weechat_config_new("tox",
                                                 tox_weechat_config_reload_callback, NULL);

    tox_weechat_config_section_identity =
        weechat_config_new_section(tox_weechat_config_file, "identity",
                                   0, 0,
                                   tox_weechat_config_identity_read_callback, NULL,
                                   tox_weechat_config_identity_write_callback, NULL,
                                   tox_weechat_config_identity_write_default_callback, NULL,
                                   NULL, NULL,
                                   NULL, NULL);


}

void
tox_weechat_config_init_identity(const char *name)
{
     struct t_config_option *option =
         weechat_config_new_option(tox_weechat_config_file,
                                   tox_weechat_config_section_identity,
                                   "name", "string",
                                   "identity name",
                                   NULL, 0, 0,
                                   name, name,
                                   0,
                                   tox_weechat_config_identity_name_check_callback, NULL,
                                   tox_weechat_config_identity_name_change_callback, NULL,
                                   NULL, NULL);
}
