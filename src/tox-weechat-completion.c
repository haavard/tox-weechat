#include <stdlib.h>
#include <inttypes.h>

#include <weechat/weechat-plugin.h>

#include "tox-weechat.h"
#include "tox-weechat-identities.h"

#include "tox-weechat-completion.h"

enum
{
    TOX_WEECHAT_ALL_IDENTITIES,
    TOX_WEECHAT_CONNECTED_IDENTITIES,
    TOX_WEECHAT_DISCONNECTED_IDENTITIES,
};

int
tox_weechat_completion_identity(void *data,
                                const char *completion_item,
                                struct t_gui_buffer *buffer,
                                struct t_gui_completion *completion)
{
    int flag = (int)(intptr_t)data;

    for (struct t_tox_weechat_identity *identity = tox_weechat_identities;
         identity;
         identity = identity->next_identity)
    {
        if (flag == TOX_WEECHAT_ALL_IDENTITIES
            || (flag == TOX_WEECHAT_CONNECTED_IDENTITIES && identity->tox != NULL)
            || (flag == TOX_WEECHAT_DISCONNECTED_IDENTITIES && identity->tox == NULL))
        {
            weechat_hook_completion_list_add(completion,
                                             identity->name,
                                             0,
                                             WEECHAT_LIST_POS_SORT);
        }
    }

    return WEECHAT_RC_OK;
}

void
tox_weechat_completion_init()
{
    weechat_hook_completion("tox_identities",
                            "identity",
                            tox_weechat_completion_identity,
                            (void *)(intptr_t)TOX_WEECHAT_ALL_IDENTITIES);
    weechat_hook_completion("tox_connected_identities",
                            "identity",
                            tox_weechat_completion_identity,
                            (void *)(intptr_t)TOX_WEECHAT_CONNECTED_IDENTITIES);
    weechat_hook_completion("tox_disconnected_identities",
                            "identity",
                            tox_weechat_completion_identity,
                            (void *)(intptr_t)TOX_WEECHAT_DISCONNECTED_IDENTITIES);
}
