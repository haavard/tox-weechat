#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>
#include <jansson.h>

#include "tox-weechat.h"
#include "tox-weechat-identities.h"
#include "tox-weechat-utils.h"

#include "tox-weechat-friend-requests.h"

void
tox_weechat_friend_request_new(struct t_tox_weechat_identity *identity,
                               const uint8_t *client_id,
                               const char *message)
{
    // check friend request count
    struct t_config_option *option =
        identity->options[TOX_WEECHAT_IDENTITY_OPTION_MAX_FRIEND_REQUESTS];
    unsigned int max_requests = weechat_config_integer(option);
    if (identity->friend_request_count >= max_requests)
    {
        weechat_printf(identity->buffer,
                       "%sReceived a friend request, but your friend request list is full!",
                       weechat_prefix("warning"));
        return;
    }

    struct t_tox_weechat_friend_request *request = malloc(sizeof(*request));
    request->identity = identity;
    request->message = strdup(message);
    memcpy(request->address, client_id, TOX_CLIENT_ID_SIZE);

    char hex_address[TOX_CLIENT_ID_SIZE * 2 + 1];
    tox_weechat_bin2hex(request->address, TOX_CLIENT_ID_SIZE, hex_address);
    weechat_printf(identity->buffer,
                   "%sReceived a friend request from %s: \"%s\"",
                   weechat_prefix("network"),
                   hex_address,
                   request->message);

    // add to list
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
    struct t_tox_weechat_identity *identity = request->identity;
    if (request == identity->last_friend_request)
        identity->last_friend_request = request->prev_request;

    if (request->prev_request)
        request->prev_request->next_request = request->next_request;
    else
        identity->friend_requests = request->next_request;

    if (request->next_request)
        request->next_request->prev_request = request->prev_request;

    --(identity->friend_request_count);
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
tox_weechat_friend_request_init_identity(struct t_tox_weechat_identity *identity)
{
    // TODO: load from file
    identity->friend_requests = identity->last_friend_request = NULL;
    identity->friend_request_count = 0;

}

void
tox_weechat_friend_request_save_identity(struct t_tox_weechat_identity *identity)
{
    for (struct t_tox_weechat_friend_request *request = identity->friend_requests;
         request;
         request = request->next_request)
    {

    }
}

void
tox_weechat_friend_request_free(struct t_tox_weechat_friend_request *request)
{
    free(request->message);
    free(request);
}

void
tox_weechat_friend_request_free_identity(struct t_tox_weechat_identity *identity)
{
    while (identity->friend_requests)
        tox_weechat_friend_request_remove(identity->friend_requests);
}
