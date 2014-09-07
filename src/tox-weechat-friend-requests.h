#ifndef TOX_WEECHAT_FRIEND_REQUESTS_H
#define TOX_WEECHAT_FRIEND_REQUESTS_H

#include <stdint.h>

#include <tox/tox.h>

struct t_tox_weechat_friend_request
{
    // public address of friend request
    uint8_t address[TOX_CLIENT_ID_SIZE];

    // message sent with request
    char *message;

    struct t_tox_weechat_identity *identity;

    struct t_tox_weechat_friend_request *next_request;
    struct t_tox_weechat_friend_request *prev_request;
};

void
tox_weechat_friend_request_add(struct t_tox_weechat_identity *identity,
                               struct t_tox_weechat_friend_request *request);

void
tox_weechat_accept_friend_request(struct t_tox_weechat_friend_request *request);

void
tox_weechat_decline_friend_request(struct t_tox_weechat_friend_request *request);

struct t_tox_weechat_friend_request *
tox_weechat_friend_request_with_num(struct t_tox_weechat_identity *identity,
                                    unsigned int num);

void
tox_weechat_friend_requests_free(struct t_tox_weechat_identity *identity);

#endif // TOX_WEECHAT_FRIEND_REQUESTS_H
