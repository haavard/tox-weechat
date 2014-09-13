#ifndef TOX_WEECHAT_CHATS_H
#define TOX_WEECHAT_CHATS_H

#include <stdint.h>

#include <tox/tox.h>

struct t_tox_weechat_chat
{
    struct t_gui_buffer *buffer;

    int32_t friend_number;

    struct t_tox_weechat_identity *identity;

    struct t_tox_weechat_chat *next_chat;
    struct t_tox_weechat_chat *prev_chat;
};

struct t_tox_weechat_chat *
tox_weechat_get_friend_chat(struct t_tox_weechat_identity *identity,
                            int32_t friend_number);

struct t_tox_weechat_chat *
tox_weechat_get_existing_friend_chat(struct t_tox_weechat_identity *identity,
                                     int32_t friend_number);

struct t_tox_weechat_chat *
tox_weechat_get_chat_for_buffer(struct t_gui_buffer *target_buffer);

void tox_weechat_chat_print_message(struct t_tox_weechat_chat *chat,
                                    const char *sender,
                                    const char *message);

void tox_weechat_chat_print_action(struct t_tox_weechat_chat *chat,
                                   const char *sender,
                                   const char *message);

void
tox_weechat_chat_refresh(struct t_tox_weechat_chat *chat);

void
tox_weechat_chat_queue_refresh(struct t_tox_weechat_chat *chat);

#endif // TOX_WEECHAT_CHATS_H
