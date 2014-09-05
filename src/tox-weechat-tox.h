#ifndef TOX_WEECHAT_TOX_H
#define TOX_WEECHAT_TOX_H

#include <tox/tox.h>

struct t_tox_weechat_identity
{
    Tox *tox;

    char *name;
    char *data_path;

    struct t_gui_buffer *buffer;

    int is_connected;

    struct t_tox_weechat_identity *next_identity;
    struct t_tox_weechat_identity *prev_identity;
};

extern struct t_tox_weechat_identity *tox_weechat_identities;

/**
 * Initialize the Tox object, bootstrap the DHT and start working.
 */
void tox_weechat_tox_init();

void tox_weechat_init_identity(struct t_tox_weechat_identity *identity);

/**
 * Bootstrap DHT using an inet address, port and a Tox address.
 */
int tox_weechat_bootstrap(char *address, uint16_t port, char *public_key);

/**
 * Dump Tox to file and de-initialize.
 */
void tox_weechat_tox_free();

#endif // TOX_WEECHAT_TOX_H
