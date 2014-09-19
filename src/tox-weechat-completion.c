#include <stdlib.h>

#include <weechat/weechat-plugin.h>

#include "tox-weechat.h"
#include "tox-weechat-identities.h"

#include "tox-weechat-completion.h"

int
tox_weechat_completion_identity(void *data,
                                const char *completion_item,
                                struct t_gui_buffer *buffer,
                                struct t_gui_completion *completion)
{
    for (struct t_tox_weechat_identity *identity = tox_weechat_identities;
         identity;
         identity = identity->next_identity)
    {
        weechat_hook_completion_list_add(completion,
                                         identity->name,
                                         0,
                                         WEECHAT_LIST_POS_SORT);
    }

    return WEECHAT_RC_OK;
}

void
tox_weechat_completion_init()
{
    weechat_hook_completion("tox_identities",
                            "identity",
                            tox_weechat_completion_identity, NULL);
}
