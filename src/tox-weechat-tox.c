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

#include "tox-weechat-tox.h"

#define INITIAL_NAME "WeeChatter"

#define BOOTSTRAP_ADDRESS "192.254.75.98"
#define BOOTSTRAP_PORT 33445
#define BOOTSTRAP_KEY "951C88B7E75C867418ACDB5D273821372BB5BD652740BCDF623A4FA293E75D2F"

Tox *tox = NULL;

/**
 * Return the path to the Tox data file. Must be freed.
 */
char *
tox_weechat_data_path()
{
    const char *weechat_dir = weechat_info_get("weechat_dir", NULL);
    const char *file_path = "/tox/data";

    weechat_mkdir_home("tox", 0755);

    int path_length = strlen(weechat_dir) + strlen(file_path) + 1;
    char *tox_data_path = malloc(path_length);

    strcpy(tox_data_path, weechat_dir);
    strcat(tox_data_path, file_path);
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
    tox_do(tox);
    weechat_hook_timer(tox_do_interval(tox), 0, 1, tox_weechat_do_timer_cb, data);

    return WEECHAT_RC_OK;
}

void
tox_weechat_friend_message_callback(Tox *tox,
                                    int32_t friend_number,
                                    const uint8_t *message,
                                    uint16_t length,
                                    void *data)
{
    struct t_tox_chat *chat = tox_weechat_get_friend_chat(friend_number);

    char *name = tox_weechat_get_name_nt(friend_number);
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
    struct t_tox_chat *chat = tox_weechat_get_friend_chat(friend_number);

    char *name = tox_weechat_get_name_nt(friend_number);
    char *message_nt = tox_weechat_null_terminate(message, length);

    tox_weechat_chat_print_action(chat, name, message_nt);

    free(name);
    free(message_nt);
}

void
tox_weechat_tox_init()
{
    tox = tox_new(0);

    // try loading Tox saved data
    char *data_file_path = tox_weechat_data_path();
    if (tox_weechat_load_file(tox, data_file_path) == -1)
    {
        // couldn't load Tox, set a default name
        tox_set_name(tox, (uint8_t *)INITIAL_NAME, strlen(INITIAL_NAME));
    }
    free(data_file_path);

    // bootstrap
    uint8_t *pub_key = tox_weechat_hex2bin(BOOTSTRAP_KEY);
    tox_bootstrap_from_address(tox, BOOTSTRAP_ADDRESS, htons(BOOTSTRAP_PORT), pub_key);
    free(pub_key);

    // start tox_do loop
    tox_weechat_do_timer_cb(NULL, 0);

    tox_callback_friend_message(tox, tox_weechat_friend_message_callback, NULL);
    tox_callback_friend_action(tox, tox_weechat_friend_action_callback, NULL);
}

void
tox_weechat_tox_free()
{
    // save Tox data to a buffer
    uint32_t size = tox_size(tox);
    uint8_t *data = malloc(sizeof(*data) * size);
    tox_save(tox, data);

    // save buffer to a file
    char *data_file_path = tox_weechat_data_path();
    FILE *data_file = fopen(data_file_path, "w");
    fwrite(data, sizeof(data[0]), size, data_file);
    fclose(data_file);
    free(data_file_path);

    // free the Tox object
    tox_kill(tox);
}

