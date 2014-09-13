#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"
#include "tox-weechat-chats.h"
#include "tox-weechat-tox-callbacks.h"
#include "tox-weechat-utils.h"

#include "tox-weechat-identities.h"

struct t_tox_weechat_identity *tox_weechat_identities = NULL;
struct t_tox_weechat_identity *tox_weechat_last_identity = NULL;

/**
 * Return the default data file path for an identity name. Must be freed.
 */
char *
tox_weechat_default_data_path(const char *name)
{
    const char *weechat_dir = weechat_info_get("weechat_dir", NULL);
    const char *tox_dir = "/tox/";

    weechat_mkdir_home("tox", 0755);

    int path_length = strlen(weechat_dir) + strlen(tox_dir) + strlen(name) + 1;
    char *tox_data_path = malloc(sizeof(*tox_data_path) + path_length);

    strcpy(tox_data_path, weechat_dir);
    strcat(tox_data_path, tox_dir);
    strcat(tox_data_path, name);
    tox_data_path[path_length-1] = 0;

    return tox_data_path;
}

int
tox_weechat_load_data_file(Tox *tox, char *path)
{
    FILE *file = fopen(path, "r");
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
        int status = tox_load(tox, data, size);

        free(data);

        return status;
    }

    return -1;
}

int
tox_weechat_save_data_file(Tox *tox, char *path)
{
    // save Tox data to a buffer
    uint32_t size = tox_size(tox);
    uint8_t *data = malloc(sizeof(*data) * size);
    tox_save(tox, data);

    // save buffer to a file
    FILE *file = fopen(path, "w");
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

    tox_weechat_identity_free(data);

    return WEECHAT_RC_OK;
}

#define BOOTSTRAP_ADDRESS "192.254.75.98"
#define BOOTSTRAP_PORT 33445
#define BOOTSTRAP_KEY "951C88B7E75C867418ACDB5D273821372BB5BD652740BCDF623A4FA293E75D2F"

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
    identity->data_file_path = tox_weechat_default_data_path(name);

    // add to identity list
    identity->prev_identity= tox_weechat_last_identity;
    identity->next_identity = NULL;

    if (tox_weechat_identities == NULL)
        tox_weechat_identities = identity;
    else
        tox_weechat_last_identity->next_identity = identity;

    tox_weechat_last_identity = identity;

    // set up internal vars
    identity->chats = identity->last_chat = NULL;
    // TODO: load from disk
    identity->friend_requests = identity->last_friend_request = NULL;
    identity->friend_request_count = 0;

    return identity;
}

void
tox_weechat_identity_connect(struct t_tox_weechat_identity *identity)
{
    // create main buffer
    identity->buffer = weechat_buffer_new(identity->name,
                                          NULL, NULL,
                                          tox_weechat_identity_buffer_close_callback, NULL);

    // create Tox
    identity->tox = tox_new(NULL);

    // try loading Tox saved data
    if (tox_weechat_load_data_file(identity->tox, identity->data_file_path) == -1)
    {
        // couldn't load Tox, set a default name
        tox_set_name(identity->tox,
                     (uint8_t *)"WeeChatter", strlen("WeeChatter"));
    }

    // bootstrap DHT
    tox_weechat_bootstrap_tox(identity->tox, BOOTSTRAP_ADDRESS,
                                             BOOTSTRAP_PORT,
                                             BOOTSTRAP_KEY);

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
    // remove from list
    if (identity == tox_weechat_last_identity)
        tox_weechat_last_identity = identity->prev_identity;

    if (identity->prev_identity)
        identity->prev_identity->next_identity = identity->next_identity;
    else
        tox_weechat_identities = identity->next_identity;

    if (identity->next_identity)
        identity->next_identity->prev_identity = identity->prev_identity;

    // save and kill tox
    int result = tox_weechat_save_data_file(identity->tox,
                                            identity->data_file_path);
    tox_kill(identity->tox);

    if (result == -1)
    {
        weechat_printf(NULL,
                       "%sCould not save Tox identity %s to file: %s",
                       weechat_prefix("error"),
                       identity->name,
                       identity->data_file_path);
    }

    if (identity->buffer)
        weechat_buffer_close(identity->buffer);

    // TODO: free config, free friend reqs/chats

    free(identity->name);
    free(identity);
}

void
tox_weechat_identity_free_all()
{
    while (tox_weechat_identities)
        tox_weechat_identity_free(tox_weechat_identities);
}
