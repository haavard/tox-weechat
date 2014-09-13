#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "tox-weechat.h"
#include "tox-weechat-identities.h"
#include "tox-weechat-utils.h"

#include "tox-weechat-friend-requests.h"

void
tox_weechat_friend_request_free(struct t_tox_weechat_friend_request *request)
{
    free(request->message);
    free(request);
}

void
tox_weechat_friend_request_add(struct t_tox_weechat_identity *identity,
                               struct t_tox_weechat_friend_request *request)
{
    request->identity = identity;

    request->prev_request = identity->last_friend_request;
    request->next_request = NULL;

    if (identity->friend_requests == NULL)
        identity->friend_requests = request;
    else
        identity->last_friend_request->next_request = request;

    identity->last_friend_request = request;
    ++(identity->friend_request_count);
}

void
tox_weechat_friend_request_remove(struct t_tox_weechat_friend_request *request)
{
    if (request->prev_request)
        request->prev_request->next_request = request->next_request;
    if (request->next_request)
        request->next_request->prev_request = request->prev_request;

    if (request == request->identity->friend_requests)
        request->identity->friend_requests = request->next_request;
    if (request == request->identity->last_friend_request)
        request->identity->last_friend_request = request->prev_request;

    tox_weechat_friend_request_free(request);
}

void
tox_weechat_accept_friend_request(struct t_tox_weechat_friend_request *request)
{
    tox_add_friend_norequest(request->identity->tox, request->address);
    tox_weechat_friend_request_remove(request);
}

void
tox_weechat_decline_friend_request(struct t_tox_weechat_friend_request *request)
{
    tox_weechat_friend_request_remove(request);
}

struct t_tox_weechat_friend_request *
tox_weechat_friend_request_with_num(struct t_tox_weechat_identity *identity,
                                    unsigned int num)
{
    if (num >= identity->friend_request_count) return NULL;

    unsigned int i = 0;
    struct t_tox_weechat_friend_request *request = identity->friend_requests;
    while (i != num && request->next_request)
    {
        request = request->next_request;
        ++i;
    }

    return request;
}

void
tox_weechat_friend_requests_init()
{
}

void
tox_weechat_friend_requests_free(struct t_tox_weechat_identity *identity)
{
    // TODO: persist requests
    while (identity->friend_requests)
        tox_weechat_friend_request_remove(identity->friend_requests);
}
