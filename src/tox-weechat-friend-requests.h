#ifndef TOX_WEECHAT_FRIEND_REQUESTS_H
#define TOX_WEECHAT_FRIEND_REQUESTS_H

#include <stdint.h>

#include <tox/tox.h>

struct t_tox_friend_request
{
    // public address of friend request
    uint8_t address[TOX_CLIENT_ID_SIZE];

    // message sent with request
    char *message;

    // linked list pointers
    struct t_tox_friend_request *next;
    struct t_tox_friend_request *prev;
};

struct t_tox_friend_request *tox_weechat_first_friend_request();

void tox_weechat_accept_friend_request(struct t_tox_friend_request *request);
void tox_weechat_decline_friend_request(struct t_tox_friend_request *request);
struct t_tox_friend_request *tox_weechat_friend_request_with_num(unsigned int num);

void tox_weechat_friend_requests_init();
void tox_weechat_friend_requests_free();

#endif // TOX_WEECHAT_FRIEND_REQUESTS_H
