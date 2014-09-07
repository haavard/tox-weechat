#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"
#include "tox-weechat-utils.h"
#include "tox-weechat-chats.h"
#include "tox-weechat-friend-requests.h"

#include "tox-weechat-tox.h"

#define INITIAL_NAME "WeeChatter"

#define BOOTSTRAP_ADDRESS "192.254.75.98"
#define BOOTSTRAP_PORT 33445
#define BOOTSTRAP_KEY "951C88B7E75C867418ACDB5D273821372BB5BD652740BCDF623A4FA293E75D2F"

struct t_tox_weechat_identity *tox_weechat_identities = NULL;
struct t_tox_weechat_identity *tox_weechat_last_identity = NULL;

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

/**
 * Load Tox data from a file path.
 *
 * Returns 0 on success, -1 on failure.  */
int
tox_weechat_load_file(Tox *tox, char *path)
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
tox_weechat_do_timer_cb(void *data,
                        int remaining_calls)
{
    struct t_tox_weechat_identity *identity = data;

    tox_do(identity->tox);
    weechat_hook_timer(tox_do_interval(identity->tox), 0, 1,
                       tox_weechat_do_timer_cb, identity);

    int connected = tox_isconnected(identity->tox);
    if (connected ^ identity->is_connected)
    {
        identity->is_connected = connected;
        weechat_bar_item_update("buffer_plugin");
    }

    return WEECHAT_RC_OK;
}

int
tox_weechat_chat_refresh_timer_callback(void *data, int remaining)
{
    struct t_tox_weechat_chat *chat = data;
    tox_weechat_chat_refresh(chat);

    return WEECHAT_RC_OK;
}

void
tox_weechat_friend_message_callback(Tox *tox,
                                    int32_t friend_number,
                                    const uint8_t *message,
                                    uint16_t length,
                                    void *data)
{
    struct t_tox_weechat_identity *identity = data;
    struct t_tox_weechat_chat *chat = tox_weechat_get_friend_chat(identity,
                                                                  friend_number);

    char *name = tox_weechat_get_name_nt(identity->tox, friend_number);
    char *message_nt = tox_weechat_null_terminate(message, length);

    tox_weechat_chat_print_message(chat, name, message_nt);

    free(name);
    free(message_nt);
}

void
tox_weechat_friend_action_callback(Tox *tox,
                                   int32_t friend_number,
                                   const uint8_t *message,
                                   uint16_t length,
                                   void *data)
{
    struct t_tox_weechat_identity *identity = data;
    struct t_tox_weechat_chat *chat = tox_weechat_get_friend_chat(identity,
                                                                  friend_number);

    char *name = tox_weechat_get_name_nt(identity->tox, friend_number);
    char *message_nt = tox_weechat_null_terminate(message, length);

    tox_weechat_chat_print_action(chat, name, message_nt);

    free(name);
    free(message_nt);
}

void
tox_weechat_connection_status_callback(Tox *tox,
                                       int32_t friend_number,
                                       uint8_t status,
                                       void *data)
{
    if (status == 1)
    {
        struct t_tox_weechat_identity *identity = data;
        char *name = tox_weechat_get_name_nt(identity->tox, friend_number);

        weechat_printf(identity->buffer,
                       "%s%s just went online!",
                       weechat_prefix("network"),
                       name);
        free(name);
    }
}

void
tox_weechat_name_change_callback(Tox *tox,
                                 int32_t friend_number,
                                 const uint8_t *name,
                                 uint16_t length,
                                 void *data)
{
    struct t_tox_weechat_identity *identity = data;
    struct t_tox_weechat_chat *chat = tox_weechat_get_existing_friend_chat(identity,
                                                                           friend_number);

    char *old_name = tox_weechat_get_name_nt(identity->tox, friend_number);
    char *new_name = tox_weechat_null_terminate(name, length);

    if (strcmp(old_name, new_name) != 0)
    {
        if (chat)
        {
            weechat_hook_timer(10, 0, 1,
                               tox_weechat_chat_refresh_timer_callback, chat);

                weechat_printf(chat->buffer,
                               "%s%s is now known as %s",
                               weechat_prefix("network"),
                               old_name, new_name);
        }
    }

    weechat_printf(identity->buffer,
                   "%s%s is now known as %s",
                   weechat_prefix("network"),
                   old_name, new_name);

    free(old_name);
    free(new_name);
}

void
tox_weechat_user_status_callback(Tox *tox,
                                 int32_t friend_number,
                                 uint8_t status,
                                 void *data)
{
    struct t_tox_weechat_identity *identity = data;
    struct t_tox_weechat_chat *chat = tox_weechat_get_existing_friend_chat(identity,
                                                                           friend_number);
    if (chat)
        weechat_hook_timer(10, 0, 1,
                           tox_weechat_chat_refresh_timer_callback, chat);
}

void
tox_weechat_status_message_callback(Tox *tox,
                                    int32_t friend_number,
                                    const uint8_t *message,
                                    uint16_t length,
                                    void *data)
{
    struct t_tox_weechat_identity *identity = data;
    struct t_tox_weechat_chat *chat = tox_weechat_get_existing_friend_chat(identity,
                                                                           friend_number);
    if (chat)
        weechat_hook_timer(10, 0, 1,
                           tox_weechat_chat_refresh_timer_callback, chat);
}

void
tox_weechat_callback_friend_request(Tox *tox,
                                    const uint8_t *public_key,
                                    const uint8_t *message,
                                    uint16_t length,
                                    void *data)
{
    struct t_tox_weechat_identity *identity = data;
    if (identity->friend_request_count >= identity->max_friend_requests)
    {
        weechat_printf(identity->buffer,
                       "%sReceived a friend request, but your friend request "
                       "list is full!",
                       weechat_prefix("warning"));
        return;
    }
    struct t_tox_weechat_friend_request *request = malloc(sizeof(*request));

    memcpy(request->address, public_key, TOX_CLIENT_ID_SIZE);
    request->message = tox_weechat_null_terminate(message, length);

    tox_weechat_friend_request_add(identity, request);

    char *hex_address = malloc(TOX_CLIENT_ID_SIZE * 2 + 1);
    tox_weechat_bin2hex(request->address, TOX_CLIENT_ID_SIZE, hex_address);

    weechat_printf(identity->buffer,
                   "%sReceived a friend request from %s: \"%s\"",
                   weechat_prefix("network"),
                   hex_address,
                   request->message);

    free(hex_address);
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

int
tox_weechat_bootstrap(char *address, uint16_t port, char *public_key)
{
    return tox_weechat_bootstrap_tox(tox_weechat_identities->tox, address, port, public_key);
}

void
tox_weechat_tox_init()
{
    struct t_tox_weechat_identity *identity = malloc(sizeof(*identity));
    identity->name = strdup("tox");
    identity->data_path = tox_weechat_data_path();

    tox_weechat_init_identity(identity);

    tox_weechat_identities = identity;
}

struct t_tox_weechat_identity *
tox_weechat_identity_new(const char *name)
{
    struct t_tox_weechat_identity *identity = malloc(sizeof(*identity));
    identity->name = strdup(name);

    // TODO: add close callback
    identity->buffer = weechat_buffer_new(identity->name, NULL, NULL, NULL, NULL);

    identity->tox = tox_new(NULL);

    identity->prev_identity= tox_weechat_last_identity;
    identity->next_identity = NULL;

    if (tox_weechat_identities == NULL)
        tox_weechat_identities = identity;
    else
        tox_weechat_last_identity->next_identity = identity;

    tox_weechat_last_identity = identity;

    return identity;
}

void
tox_weechat_init_identity(struct t_tox_weechat_identity *identity)
{
    // try loading Tox saved data
    if (tox_weechat_load_file(identity->tox, identity->data_path) == -1)
    {
        // couldn't load Tox, set a default name
        tox_set_name(identity->tox,
                     (uint8_t *)INITIAL_NAME, strlen(INITIAL_NAME));
    }

    // bootstrap DHT
    tox_weechat_bootstrap_tox(identity->tox, BOOTSTRAP_ADDRESS,
                                             BOOTSTRAP_PORT,
                                             BOOTSTRAP_KEY);

    // start tox_do loop
    tox_weechat_do_timer_cb(identity, 0);

    // register tox callbacks
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
tox_weechat_save_to_file(Tox *tox, char *path)
{
    // save Tox data to a buffer
    uint32_t size = tox_size(tox);
    uint8_t *data = malloc(sizeof(*data) * size);
    tox_save(tox, data);

    // save buffer to a file
    FILE *data_file = fopen(path, "w");
    fwrite(data, sizeof(data[0]), size, data_file);
    fclose(data_file);
}

void
tox_weechat_free_identity(struct t_tox_weechat_identity *identity)
{
    tox_weechat_save_to_file(identity->tox,
                             identity->data_path);

    tox_kill(identity->tox);
    free(identity->name);
    free(identity->data_path);
    free(identity);
}

void
tox_weechat_tox_free()
{
    tox_weechat_free_identity(tox_weechat_identities);
}

