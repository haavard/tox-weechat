#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"
#include "tox-weechat-identities.h"
#include "tox-weechat-chats.h"
#include "tox-weechat-friend-requests.h"
#include "tox-weechat-utils.h"

#include "tox-weechat-tox-callbacks.h"

int
tox_weechat_do_timer_cb(void *data,
                        int remaining_calls)
{
    struct t_tox_weechat_identity *identity = data;

    tox_do(identity->tox);
    weechat_hook_timer(tox_do_interval(identity->tox), 0, 1,
                       tox_weechat_do_timer_cb, identity);

    // check connection status
    int connected = tox_isconnected(identity->tox);
    if (connected ^ identity->is_connected)
    {
        identity->is_connected = connected;
    }

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

    if (chat && strcmp(old_name, new_name) != 0)
    {
        tox_weechat_chat_queue_refresh(chat);

        weechat_printf(chat->buffer,
                       "%s%s is now known as %s",
                       weechat_prefix("network"),
                       old_name, new_name);
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
        tox_weechat_chat_queue_refresh(chat);
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
        tox_weechat_chat_queue_refresh(chat);
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
                       "%sReceived a friend request, but your friend request list is full!",
                       weechat_prefix("warning"));
        return;
    }

    // TODO: move to t-w-f-r.h
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

