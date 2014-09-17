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

    if (identity->tox)
    {
        tox_do(identity->tox);
        struct t_hook *hook = weechat_hook_timer(tox_do_interval(identity->tox), 0, 1,
                                                 tox_weechat_do_timer_cb, identity);
        identity->tox_do_timer = hook;

        // check connection status
        int connected = tox_isconnected(identity->tox);
        if (connected ^ identity->tox_online)
        {
            identity->tox_online = connected;
            weechat_bar_item_update("buffer_plugin");
        }
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
    struct t_tox_weechat_identity *identity = data;
    char *name = tox_weechat_get_name_nt(identity->tox, friend_number);

    if (status == 0)
    {
        weechat_printf(identity->buffer,
                       "%s%s just went offline.",
                       weechat_prefix("network"),
                       name);
    }
    else if (status == 1)
    {
        weechat_printf(identity->buffer,
                       "%s%s just came online.",
                       weechat_prefix("network"),
                       name);
    }
    free(name);
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
    char *message_nt = tox_weechat_null_terminate(message, length);
    tox_weechat_friend_request_new(identity, public_key, message_nt);
    free(message_nt);
}

