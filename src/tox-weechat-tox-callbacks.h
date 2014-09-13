#ifndef TOX_WEECHAT_TOX_CALLBACKS_H
#define TOX_WEECHAT_TOX_CALLBACKS_H

#include <stdint.h>

#include <tox/tox.h>

int
tox_weechat_do_timer_cb(void *data,
                        int remaining_calls);

void
tox_weechat_friend_message_callback(Tox *tox,
                                    int32_t friend_number,
                                    const uint8_t *message,
                                    uint16_t length,
                                    void *data);

void
tox_weechat_friend_action_callback(Tox *tox,
                                   int32_t friend_number,
                                   const uint8_t *message,
                                   uint16_t length,
                                   void *data);

void
tox_weechat_connection_status_callback(Tox *tox,
                                       int32_t friend_number,
                                       uint8_t status,
                                       void *data);

void
tox_weechat_name_change_callback(Tox *tox,
                                 int32_t friend_number,
                                 const uint8_t *name,
                                 uint16_t length,
                                 void *data);

void
tox_weechat_user_status_callback(Tox *tox,
                                 int32_t friend_number,
                                 uint8_t status,
                                 void *data);

void
tox_weechat_status_message_callback(Tox *tox,
                                    int32_t friend_number,
                                    const uint8_t *message,
                                    uint16_t length,
                                    void *data);

void
tox_weechat_callback_friend_request(Tox *tox,
                                    const uint8_t *public_key,
                                    const uint8_t *message,
                                    uint16_t length,
                                    void *data);

#endif // TOX_WEECHAT_TOX_CALLBACKS_H
