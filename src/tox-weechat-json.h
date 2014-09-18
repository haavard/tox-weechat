#ifndef TOX_WEECHAT_JSON_H
#define TOX_WEECHAT_JSON_H

#include <jansson.h>

#include "tox-weechat-identities.h"

extern json_t *tox_weechat_json_config;

extern const char *tox_weechat_json_key_friend_requests;
extern const char *tox_weechat_json_friend_request_key_client_id;
extern const char *tox_weechat_json_friend_request_key_message;

void
tox_weechat_json_init();

int
tox_weechat_json_save();

json_t *
tox_weechat_json_get_identity_object(struct t_tox_weechat_identity *identity);

int
tox_weechat_json_set_identity_object(struct t_tox_weechat_identity *identity,
                                     json_t *object);

#endif // TOX_WEECHAT_JSON_H
