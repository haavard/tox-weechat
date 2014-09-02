#ifndef TOX_WEECHAT_CHATS_H
#define TOX_WEECHAT_CHATS_H

#include <stdint.h>

#include <tox/tox.h>

struct t_tox_chat
{
    struct t_gui_buffer *buffer;

    int32_t friend_number;

    // linked list pointers
    struct t_tox_chat *next;
    struct t_tox_chat *prev;
};

struct t_tox_chat *tox_weechat_get_friend_chat(int32_t friend_number);
struct t_tox_chat *tox_weechat_get_existing_friend_chat(int32_t friend_number);
struct t_tox_chat *tox_weechat_get_chat_for_buffer(struct t_gui_buffer *target_buffer);

struct t_tox_chat *tox_weechat_get_first_chat();

void tox_weechat_chat_print_message(struct t_tox_chat *chat,
                                    const char *sender,
                                    const char *message);
void tox_weechat_chat_print_action(struct t_tox_chat *chat,
                                   const char *sender,
                                   const char *message);

void tox_weechat_chat_refresh(struct t_tox_chat *chat);

#endif // TOX_WEECHAT_CHATS_H
