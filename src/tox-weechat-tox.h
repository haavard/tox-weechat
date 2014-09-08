#ifndef TOX_WEECHAT_TOX_H
#define TOX_WEECHAT_TOX_H

#include <stdint.h>

struct t_tox_weechat_identity
{
    struct Tox *tox;

    char *name;
    char *data_path;

    struct t_gui_buffer *buffer;

    int is_connected;

    struct t_tox_weechat_chat *chats;
    struct t_tox_weechat_chat *last_chat;

    struct t_tox_weechat_friend_request *friend_requests;
    struct t_tox_weechat_friend_request *last_friend_request;
    unsigned int friend_request_count;
    unsigned int max_friend_requests;

    struct t_tox_weechat_identity *next_identity;
    struct t_tox_weechat_identity *prev_identity;
};

extern struct t_tox_weechat_identity *tox_weechat_identities;
extern struct t_tox_weechat_identity *tox_weechat_last_identity;

/**
 * Initialize the Tox object, bootstrap the DHT and start working.
 */
void tox_weechat_tox_init();

struct t_tox_weechat_identity *tox_weechat_identity_for_buffer(struct t_gui_buffer *buffer);

struct t_tox_weechat_identity *
tox_weechat_identity_new(const char *name);

/**
 * Bootstrap DHT using an inet address, port and a Tox address.
 */
int tox_weechat_bootstrap(char *address, uint16_t port, char *public_key);
int tox_weechat_bootstrap_tox(struct Tox *tox, char *address, uint16_t port, char *public_key);

/**
 * Dump Tox to file and de-initialize.
 */
void tox_weechat_tox_free();

#endif // TOX_WEECHAT_TOX_H
