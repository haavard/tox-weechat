#ifndef TOX_WEECHAT_IDENTITIES_H
#define TOX_WEECHAT_IDENTITIES_H

#include <stdint.h>

enum t_tox_weechat_identity_option
{
    TOX_WEECHAT_IDENTITY_OPTION_SAVEFILE = 0,
    TOX_WEECHAT_IDENTITY_OPTION_AUTOCONNECT,
    TOX_WEECHAT_IDENTITY_OPTION_MAX_FRIEND_REQUESTS,

    TOX_WEECHAT_IDENTITY_NUM_OPTIONS,
};

struct t_tox_weechat_identity
{
    struct Tox *tox;

    char *name;

    struct t_config_option *options[TOX_WEECHAT_IDENTITY_NUM_OPTIONS];

    // TODO: move to option
    char *data_file_path;
    unsigned int max_friend_requests;

    struct t_gui_buffer *buffer;

    int is_connected;

    struct t_tox_weechat_chat *chats;
    struct t_tox_weechat_chat *last_chat;

    struct t_tox_weechat_friend_request *friend_requests;
    struct t_tox_weechat_friend_request *last_friend_request;
    unsigned int friend_request_count;

    struct t_tox_weechat_identity *next_identity;
    struct t_tox_weechat_identity *prev_identity;
};

extern struct t_tox_weechat_identity *tox_weechat_identities;
extern struct t_tox_weechat_identity *tox_weechat_last_identity;

struct t_tox_weechat_identity *
tox_weechat_identity_new(const char *name);

void
tox_weechat_identity_connect(struct t_tox_weechat_identity *identity);

struct t_tox_weechat_identity *
tox_weechat_identity_for_buffer(struct t_gui_buffer *buffer);

void
tox_weechat_identity_free(struct t_tox_weechat_identity *identity);

void
tox_weechat_identity_free_all();

#endif // TOX_WEECHAT_IDENTITIES_H
