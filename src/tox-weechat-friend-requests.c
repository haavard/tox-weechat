#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"
#include "tox-weechat-utils.h"

#include "tox-weechat-friend-requests.h"

#define MAX_FRIEND_REQUESTS 250

struct t_tox_friend_request *tox_weechat_friend_requests_head = NULL;
struct t_tox_friend_request *tox_weechat_friend_requests_tail = NULL;
unsigned int friend_request_count = 0;

void
tox_weechat_friend_request_free(struct t_tox_friend_request *request)
{
    free(request->message);
    free(request);
}

void
tox_weechat_friend_request_add(struct t_tox_friend_request *request)
{
    if (tox_weechat_friend_requests_head == NULL)
    {
        tox_weechat_friend_requests_head = tox_weechat_friend_requests_tail = request;
        request->prev = request->next = NULL;
        friend_request_count = 1;
    }
    else
    {
        tox_weechat_friend_requests_tail->next = request;
        request->prev = tox_weechat_friend_requests_tail;
        request->next = NULL;

        tox_weechat_friend_requests_tail = request;

        ++friend_request_count;
    }
}

void
tox_weechat_friend_request_remove(struct t_tox_friend_request *request)
{
    if (request->prev)
        request->prev->next = request->next;
    if (request->next)
        request->next->prev = request->prev;

    if (request == tox_weechat_friend_requests_head)
        tox_weechat_friend_requests_head = request->next;
    if (request == tox_weechat_friend_requests_tail)
        tox_weechat_friend_requests_tail = request->prev;

    tox_weechat_friend_request_free(request);
}

struct t_tox_friend_request *
tox_weechat_first_friend_request()
{
    return tox_weechat_friend_requests_head;
}

unsigned int
tox_weechat_friend_request_count()
{
    return friend_request_count;
}

void
tox_weechat_accept_friend_request(struct t_tox_friend_request *request)
{
    tox_add_friend_norequest(tox, request->address);
    tox_weechat_friend_request_remove(request);
}

void
tox_weechat_decline_friend_request(struct t_tox_friend_request *request)
{
    tox_weechat_friend_request_remove(request);
}

struct t_tox_friend_request *
tox_weechat_friend_request_with_num(unsigned int num)
{
    if (num < 1 || num > friend_request_count) return NULL;

    unsigned int i = 1;
    struct t_tox_friend_request *request = tox_weechat_friend_requests_head;
    while (i != num && request->next)
    {
        request = request->next;
        ++i;
    }

    return request;
}

void
tox_weechat_callback_friend_request(Tox *tox,
                                    const uint8_t *public_key,
                                    const uint8_t *message,
                                    uint16_t length,
                                    void *userdata)
{
    if (friend_request_count >= MAX_FRIEND_REQUESTS)
    {
        weechat_printf(tox_main_buffer,
                       "%sReceived a friend request, but your friend request "
                       "list is full!",
                       weechat_prefix("warning"));
        return;
    }
    struct t_tox_friend_request *request = malloc(sizeof(*request));

    memcpy(request->address, public_key, TOX_CLIENT_ID_SIZE);
    request->message = tox_weechat_null_terminate(message, length);

    tox_weechat_friend_request_add(request);

    char *hex_address = tox_weechat_bin2hex(request->address,
                                            TOX_CLIENT_ID_SIZE);
    weechat_printf(tox_main_buffer,
                   "%sReceived a friend request from %s: \"%s\"",
                   weechat_prefix("network"),
                   hex_address,
                   request->message);
}

void
tox_weechat_friend_requests_init()
{
    tox_callback_friend_request(tox,
                                tox_weechat_callback_friend_request,
                                NULL);
}

void
tox_weechat_friend_requests_free()
{
    // TODO: persist requests
    while (tox_weechat_friend_requests_head)
        tox_weechat_friend_request_remove(tox_weechat_friend_requests_head);
}
