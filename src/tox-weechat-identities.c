#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <unistd.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"
#include "tox-weechat-config.h"
#include "tox-weechat-chats.h"
#include "tox-weechat-tox-callbacks.h"
#include "tox-weechat-utils.h"

#include "tox-weechat-identities.h"

struct t_tox_weechat_identity *tox_weechat_identities = NULL;
struct t_tox_weechat_identity *tox_weechat_last_identity = NULL;

char *tox_weechat_bootstrap_addresses[] = {
    "192.254.75.98",
    "107.161.17.51",
    "144.76.60.215",
    "23.226.230.47",
    "37.59.102.176",
    "37.187.46.132",
    "178.21.112.187",
    "192.210.149.121",
    "54.199.139.199",
    "195.154.119.113",
};

int tox_weechat_bootstrap_ports[] = {
    33445, 33445, 33445, 33445, 33445,
    33445, 33445, 33445, 33445, 33445,
};

char *tox_weechat_bootstrap_keys[] = {
    "951C88B7E75C867418ACDB5D273821372BB5BD652740BCDF623A4FA293E75D2F",
    "7BE3951B97CA4B9ECDDA768E8C52BA19E9E2690AB584787BF4C90E04DBB75111",
    "04119E835DF3E78BACF0F84235B300546AF8B936F035185E2A8E9E0A67C8924F",
    "A09162D68618E742FFBCA1C2C70385E6679604B2D80EA6E84AD0996A1AC8A074",
    "B98A2CEAA6C6A2FADC2C3632D284318B60FE5375CCB41EFA081AB67F500C1B0B",
    "5EB67C51D3FF5A9D528D242B669036ED2A30F8A60E674C45E7D43010CB2E1331",
    "4B2C19E924972CB9B57732FB172F8A8604DE13EEDA2A6234E348983344B23057",
    "F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038B9D822E8460FAB67",
    "7F9C31FE850E97CEFD4C4591DF93FC757C7C12549DDD55F8EEAECC34FE76C029",
    "E398A69646B8CEACA9F0B84F553726C1C49270558C57DF5F3C368F05A7D71354",
};

char *
tox_weechat_identity_data_file_path(struct t_tox_weechat_identity *identity)
{
    // expand path
    const char *weechat_dir = weechat_info_get ("weechat_dir", NULL);
    const char *base_path = weechat_config_string(identity->options[TOX_WEECHAT_IDENTITY_OPTION_SAVEFILE]);
    char *home_expanded = weechat_string_replace(base_path, "%h", weechat_dir);
    char *full_path = weechat_string_replace(home_expanded, "%n", identity->name);
    free(home_expanded);

    return full_path;
}

int
tox_weechat_load_identity_data_file(struct t_tox_weechat_identity *identity)
{
    char *full_path = tox_weechat_identity_data_file_path(identity);

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
        int status = tox_load(identity->tox, data, size);
        free(data);

        return status;
    }

    return -1;
}

int
tox_weechat_save_identity_data_file(struct t_tox_weechat_identity *identity)
{
    char *full_path = tox_weechat_identity_data_file_path(identity);

    char *rightmost_slash = strrchr(full_path, '/');
    char *save_dir = strndup(full_path, rightmost_slash - full_path);
    weechat_mkdir_parents(save_dir, 0755);
    free(save_dir);

    // save Tox data to a buffer
    uint32_t size = tox_size(identity->tox);
    uint8_t *data = malloc(sizeof(*data) * size);
    tox_save(identity->tox, data);

    // save buffer to a file
    FILE *file = fopen(full_path, "w");
    if (file)
    {
        fwrite(data, sizeof(data[0]), size, file);
        fclose(file);

        return 0;
    }

    return -1;
}

int
tox_weechat_identity_buffer_close_callback(void *data,
                                           struct t_gui_buffer *buffer)
{
    struct t_tox_weechat_identity *identity = data;
    identity->buffer = NULL;

    tox_weechat_identity_disconnect(identity);

    return WEECHAT_RC_OK;
}


int
tox_weechat_bootstrap_tox(Tox *tox, char *address, uint16_t port, char *public_key)
{
    char *binary_key = malloc(TOX_FRIEND_ADDRESS_SIZE);
    tox_weechat_hex2bin(public_key, binary_key);

    int result = tox_bootstrap_from_address(tox,
                                            address,
                                            htons(port),
                                            (uint8_t *)binary_key);
    free(binary_key);

    return result;
}


struct t_tox_weechat_identity *
tox_weechat_identity_new(const char *name)
{
    struct t_tox_weechat_identity *identity = malloc(sizeof(*identity));
    identity->name = strdup(name);

    // add to identity list
    identity->prev_identity= tox_weechat_last_identity;
    identity->next_identity = NULL;

    if (tox_weechat_identities == NULL)
        tox_weechat_identities = identity;
    else
        tox_weechat_last_identity->next_identity = identity;

    tox_weechat_last_identity = identity;

    // set up internal vars
    identity->tox = NULL;
    identity->buffer = NULL;
    identity->tox_do_timer = NULL;
    identity->chats = identity->last_chat = NULL;

    // TODO: load from disk
    identity->friend_requests = identity->last_friend_request = NULL;
    identity->friend_request_count = 0;

    // set up config
    tox_weechat_config_init_identity(identity);

    return identity;
}

void
tox_weechat_identity_connect(struct t_tox_weechat_identity *identity)
{
    if (identity->tox)
        return;

    // create main buffer
    identity->buffer = weechat_buffer_new(identity->name,
                                          NULL, NULL,
                                          tox_weechat_identity_buffer_close_callback, identity);

    // create Tox
    identity->tox = tox_new(NULL);

    // try loading Tox saved data
    if (tox_weechat_load_identity_data_file(identity) == -1)
    {
        // we failed to load - set an initial name
        char *name;

        struct passwd *user_pwd;
        if ((user_pwd = getpwuid(geteuid())))
            name = user_pwd->pw_name;
        else
            name = "Tox User";

        tox_set_name(identity->tox,
                     (uint8_t *)name, strlen(name));
    }

    // bootstrap DHT
    int bootstrap_count = sizeof(tox_weechat_bootstrap_addresses)/sizeof(tox_weechat_bootstrap_addresses[0]);
    for (int i = 0; i < bootstrap_count; ++i)
        tox_weechat_bootstrap_tox(identity->tox, tox_weechat_bootstrap_addresses[i],
                                                 tox_weechat_bootstrap_ports[i],
                                                 tox_weechat_bootstrap_addresses[i]);

    // start Tox_do loop
    tox_weechat_do_timer_cb(identity, 0);

    // register Tox callbacks
    tox_callback_friend_message(identity->tox, tox_weechat_friend_message_callback, identity);
    tox_callback_friend_action(identity->tox, tox_weechat_friend_action_callback, identity);
    tox_callback_connection_status(identity->tox, tox_weechat_connection_status_callback, identity);
    tox_callback_name_change(identity->tox, tox_weechat_name_change_callback, identity);
    tox_callback_user_status(identity->tox, tox_weechat_user_status_callback, identity);
    tox_callback_status_message(identity->tox, tox_weechat_status_message_callback, identity);
    tox_callback_friend_request(identity->tox, tox_weechat_callback_friend_request, identity);
}

void
tox_weechat_identity_disconnect(struct t_tox_weechat_identity *identity)
{
    // check that we're not already disconnected
    if (!identity->tox)
        return;

    // save and kill tox
    int result = tox_weechat_save_identity_data_file(identity);
    tox_kill(identity->tox);
    identity->tox = NULL;

    if (result == -1)
    {
        char *path = tox_weechat_identity_data_file_path(identity);
        weechat_printf(NULL,
                       "%s%s: Could not save Tox identity %s to file: %s",
                       weechat_prefix("error"),
                       weechat_plugin->name,
                       identity->name,
                       path);
        free(path);
    }

    // stop Tox timer
    weechat_unhook(identity->tox_do_timer);
}

void
tox_weechat_identity_autoconnect()
{
    for (struct t_tox_weechat_identity *identity = tox_weechat_identities;
         identity;
         identity = identity->next_identity)
    {
        if (weechat_config_boolean(identity->options[TOX_WEECHAT_IDENTITY_OPTION_AUTOCONNECT]))
            tox_weechat_identity_connect(identity);
    }
}

struct t_tox_weechat_identity *
tox_weechat_identity_name_search(const char *name)
{
    for (struct t_tox_weechat_identity *identity = tox_weechat_identities;
         identity;
         identity = identity->next_identity)
    {
        if (weechat_strcasecmp(identity->name, name) == 0)
            return identity;
    }

    return NULL;
}

struct t_tox_weechat_identity *
tox_weechat_identity_for_buffer(struct t_gui_buffer *buffer)
{
    for (struct t_tox_weechat_identity *identity = tox_weechat_identities;
         identity;
         identity = identity->next_identity)
    {
        if (identity->buffer == buffer)
            return identity;

        for (struct t_tox_weechat_chat *chat = identity->chats;
             chat;
             chat = chat->next_chat)
        {
            if (chat->buffer == buffer)
                return identity;
        }
    }

    return NULL;
}

void
tox_weechat_identity_free(struct t_tox_weechat_identity *identity)
{
    // disconnect
    tox_weechat_identity_disconnect(identity);

    // remove from list
    if (identity == tox_weechat_last_identity)
        tox_weechat_last_identity = identity->prev_identity;

    if (identity->prev_identity)
        identity->prev_identity->next_identity = identity->next_identity;
    else
        tox_weechat_identities = identity->next_identity;

    if (identity->next_identity)
        identity->next_identity->prev_identity = identity->prev_identity;

    // TODO: free more things

    free(identity->name);
    free(identity);
}

void
tox_weechat_identity_free_all()
{
    while (tox_weechat_identities)
        tox_weechat_identity_free(tox_weechat_identities);
}
