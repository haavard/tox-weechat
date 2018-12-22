/*
 * Copyright (c) 2018 Håvard Pettersson <mail@haavard.me>
 *
 * This file is part of Tox-WeeChat.
 *
 * Tox-WeeChat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Tox-WeeChat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tox-WeeChat.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <inttypes.h>
#include <string.h>

#include <tox/tox.h>
#include <weechat/weechat-plugin.h>

#ifdef TOXAV_ENABLED
#include <tox/toxav.h>
#endif /* TOXAV_ENABLED */

#include "twc-chat.h"
#include "twc-friend-request.h"
#include "twc-group-invite.h"
#include "twc-message-queue.h"
#include "twc-profile.h"
#include "twc-tfer.h"
#include "twc-utils.h"
#include "twc.h"

#include "twc-tox-callbacks.h"

#define TWC_TFER_FILE_UPDATE_STATUS(st)                                        \
    do                                                                         \
    {                                                                          \
        file->status = st;                                                     \
        twc_tfer_file_update(profile->tfer, file);                             \
    } while (0)

int
twc_do_timer_cb(const void *pointer, void *data, int remaining_calls)
{
    uint32_t interval;
    int i;
    int64_t rc;
    char *tags;

    /* TODO: don't strip the const */
    struct t_twc_profile *profile = (void *)pointer;

    interval = tox_iteration_interval(profile->tox);
    tox_iterate(profile->tox, profile);
    struct t_hook *hook =
        weechat_hook_timer(interval, 0, 1, twc_do_timer_cb, profile, NULL);
    profile->tox_do_timer = hook;

    /* check connection status */
    TOX_CONNECTION connection = tox_self_get_connection_status(profile->tox);
    bool is_connected =
        connection == TOX_CONNECTION_TCP || connection == TOX_CONNECTION_UDP;
    twc_profile_set_online_status(profile, is_connected);

    if (TWC_PROFILE_OPTION_BOOLEAN(profile, TWC_PROFILE_OPTION_AUTOJOIN))
    {
        struct t_twc_group_chat_invite *invite;
        for (i = 0; (invite = twc_group_chat_invite_with_index(profile, i));
             i++)
            if (invite->autojoin_delay <= 0)
            {
                struct t_twc_chat *friend_chat = twc_chat_search_friend(
                    profile, invite->friend_number, false);
                char *friend_name =
                    twc_get_name_nt(profile->tox, invite->friend_number);
                char *type_str;
                switch (invite->group_chat_type)
                {
                    case TOX_CONFERENCE_TYPE_TEXT:
                        type_str = "a text-only group chat";
                        break;
                    case TOX_CONFERENCE_TYPE_AV:
                        type_str = "an audio/vikdeo group chat";
                        break;
                    default:
                        type_str = "a group chat";
                        break;
                }

                rc = twc_group_chat_invite_join(invite);
                // item will be deleted, backtrack
                i--;
                if (rc >= 0)
                {
                    tags = "notify_private";
                    if (friend_chat)
                    {
                        weechat_printf_date_tags(
                            friend_chat->buffer, 0, tags,
                            "%sWe joined the %s%s%s's invite to %s.",
                            weechat_prefix("network"),
                            weechat_color("chat_nick_other"), friend_name,
                            weechat_color("reset"), type_str);
                        tags = "";
                    }
                    weechat_printf_date_tags(
                        profile->buffer, 0, tags,
                        "%sWe joined the %s%s%s's invite to %s.",
                        weechat_prefix("network"),
                        weechat_color("chat_nick_other"), friend_name,
                        weechat_color("reset"), type_str);
                }
                else
                {
                    tags = "notify_highlight";
                    if (friend_chat)
                    {
                        weechat_printf_date_tags(
                            friend_chat->buffer, 0, tags,
                            "%s%s%s%s invites you to join %s, but we failed to "
                            "process the invite. Please try again.",
                            weechat_prefix("network"),
                            weechat_color("chat_nick_other"), friend_name,
                            weechat_color("reset"));
                        tags = "";
                    }
                    weechat_printf_date_tags(
                        profile->buffer, 0, tags,
                        "%s%s%s%s invites you to join %s, but we failed to "
                        "process the invite. Please try again.",
                        weechat_prefix("network"),
                        weechat_color("chat_nick_other"), friend_name,
                        weechat_color("reset"));
                }
            }
            else
                invite->autojoin_delay -= interval;
    }

    return WEECHAT_RC_OK;
}

void
twc_friend_message_callback(Tox *tox, uint32_t friend_number,
                            TOX_MESSAGE_TYPE type, const uint8_t *message,
                            size_t length, void *data)
{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat =
        twc_chat_search_friend(profile, friend_number, true);

    char *name = twc_get_name_nt(profile->tox, friend_number);
    char *message_nt = twc_null_terminate(message, length);

    twc_chat_print_message(chat, "notify_private",
                           weechat_color("chat_nick_other"), name, message_nt,
                           type);

    free(name);
    free(message_nt);
}

void
twc_connection_status_callback(Tox *tox, uint32_t friend_number,
                               TOX_CONNECTION status, void *data)
{
    struct t_twc_profile *profile = data;
    char *name = twc_get_name_nt(profile->tox, friend_number);
    struct t_gui_nick *nick = NULL;
    struct t_twc_chat *chat =
        twc_chat_search_friend(profile, friend_number, false);

    /* TODO: print in friend's buffer if it exists */
    if (status == TOX_CONNECTION_NONE)
    {
        nick = weechat_nicklist_search_nick(profile->buffer,
                                            profile->nicklist_group, name);
        if (nick)
            weechat_nicklist_remove_nick(profile->buffer, nick);

        weechat_printf(profile->buffer, "%s%s just went offline.",
                       weechat_prefix("network"), name);
        if (chat)
        {
            weechat_printf(chat->buffer, "%s%s just went offline.",
                           weechat_prefix("network"), name);
        }
    }
    else if ((status == TOX_CONNECTION_TCP) || (status == TOX_CONNECTION_UDP))
    {
        weechat_nicklist_add_nick(profile->buffer, profile->nicklist_group,
                                  name, NULL, NULL, NULL, 1);

        weechat_printf(profile->buffer, "%s%s just came online.",
                       weechat_prefix("network"), name);
        if (chat)
        {
            weechat_printf(chat->buffer, "%s%s just came online.",
                           weechat_prefix("network"), name);
        }
        twc_message_queue_flush_friend(profile, friend_number);
    }
    free(name);
}

void
twc_name_change_callback(Tox *tox, uint32_t friend_number, const uint8_t *name,
                         size_t length, void *data)
{
    struct t_twc_profile *profile = data;
    struct t_gui_nick *nick = NULL;
    struct t_twc_chat *chat =
        twc_chat_search_friend(profile, friend_number, false);

    char *old_name = twc_get_name_nt(profile->tox, friend_number);
    char *new_name = twc_null_terminate(name, length);

    if (strcmp(old_name, new_name) != 0)
    {
        if (chat)
        {
            twc_chat_queue_refresh(chat);

            weechat_printf(chat->buffer, "%s%s is now known as %s",
                           weechat_prefix("network"), old_name, new_name);
        }

        nick = weechat_nicklist_search_nick(profile->buffer,
                                            profile->nicklist_group, old_name);
        if (nick)
            weechat_nicklist_remove_nick(profile->buffer, nick);

        weechat_nicklist_add_nick(profile->buffer, profile->nicklist_group,
                                  new_name, NULL, NULL, NULL, 1);

        weechat_printf(profile->buffer, "%s%s is now known as %s",
                       weechat_prefix("network"), old_name, new_name);
        if (profile->tfer->buffer)
        {
            size_t index;
            struct t_twc_list_item *item;
            struct t_twc_tfer_file *file;
            twc_list_foreach (profile->tfer->files, index, item)
            {
                file = item->file;
                if (file->friend_number == friend_number)
                {
                    free(file->nickname);
                    file->nickname = strdup(new_name);
                    twc_tfer_file_update(profile->tfer, item->file);
                }
            }
        }
    }

    free(old_name);
    free(new_name);
}

void
twc_user_status_callback(Tox *tox, uint32_t friend_number,
                         TOX_USER_STATUS status, void *data)
{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat =
        twc_chat_search_friend(profile, friend_number, false);
    if (chat)
        twc_chat_queue_refresh(chat);
}

void
twc_status_message_callback(Tox *tox, uint32_t friend_number,
                            const uint8_t *message, size_t length, void *data)
{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat =
        twc_chat_search_friend(profile, friend_number, false);
    if (chat)
        twc_chat_queue_refresh(chat);
}

void
twc_friend_request_callback(Tox *tox, const uint8_t *public_key,
                            const uint8_t *message, size_t length, void *data)
{
    struct t_twc_profile *profile = data;

    char *message_nt = twc_null_terminate(message, length);
    int rc = twc_friend_request_add(profile, public_key, message_nt);

    if (rc == -1)
    {
        weechat_printf_date_tags(profile->buffer, 0, "notify_highlight",
                                 "%sReceived a friend request, but your friend "
                                 "request list is full!",
                                 weechat_prefix("warning"));
    }
    else
    {
        char hex_address[TOX_PUBLIC_KEY_SIZE * 2 + 1];
        twc_bin2hex(public_key, TOX_PUBLIC_KEY_SIZE, hex_address);

        weechat_printf_date_tags(
            profile->buffer, 0, "notify_highlight",
            "%sReceived a friend request from %s with message \"%s\"; "
            "accept it with \"/friend accept %d\"",
            weechat_prefix("network"), hex_address, message_nt, rc);

        if (rc == -2)
        {
            weechat_printf_date_tags(
                profile->buffer, 0, "notify_highlight",
                "%sFailed to save friend request, try manually "
                "accepting with /friend add",
                weechat_prefix("error"));
        }
    }

    free(message_nt);
}

void
twc_group_invite_callback(Tox *tox, uint32_t friend_number,
                          TOX_CONFERENCE_TYPE type, const uint8_t *invite_data,
                          size_t length, void *data)
{
    TOX_ERR_CONFERENCE_JOIN err = TOX_ERR_CONFERENCE_JOIN_OK;
    struct t_twc_profile *profile = data;
    char *friend_name = twc_get_name_nt(profile->tox, friend_number);
    struct t_twc_chat *friend_chat =
        twc_chat_search_friend(profile, friend_number, false);
    int64_t rc;
    char *tags;

    char *type_str;
    switch (type)
    {
        case TOX_CONFERENCE_TYPE_TEXT:
            type_str = "a text-only group chat";
            break;
        case TOX_CONFERENCE_TYPE_AV:
            type_str = "an audio/vikdeo group chat";
            break;
        default:
            type_str = "a group chat";
            break;
    }

    rc = twc_group_chat_invite_add(profile, friend_number, type,
                                   (uint8_t *)invite_data, length);

    if (rc >= 0)
    {
        tags = "notify_highlight";
        if (friend_chat)
        {
            weechat_printf_date_tags(
                friend_chat->buffer, 0, tags,
                "%s%s%s%s invites you to join %s. Type "
                "\"/group join %d\" to accept.",
                weechat_prefix("network"), weechat_color("chat_nick_other"),
                friend_name, weechat_color("reset"), type_str, rc);
            tags = "";
        }
        weechat_printf_date_tags(profile->buffer, 0, tags,
                                 "%s%s%s%s invites you to join %s. Type "
                                 "\"/group join %d\" to accept.",
                                 weechat_prefix("network"),
                                 weechat_color("chat_nick_other"), friend_name,
                                 weechat_color("reset"), type_str, rc);
    }
    else
    {
        tags = "notify_highlight";
        if (friend_chat)
        {
            weechat_printf_date_tags(
                friend_chat->buffer, 0, tags,
                "%s%s%s%s invites you to join %s, but we failed to "
                "process the invite with error %d. Please try again.",
                weechat_prefix("network"), weechat_color("chat_nick_other"),
                friend_name, weechat_color("reset"), rc, err);
            tags = "";
        }
        weechat_printf_date_tags(
            profile->buffer, 0, tags,
            "%s%s%s%s invites you to join %s, but we failed to "
            "process the invite. Please try again.",
            weechat_prefix("network"), weechat_color("chat_nick_other"),
            friend_name, weechat_color("reset"), rc);
    }
    free(friend_name);
}

void
twc_handle_group_message(Tox *tox, int32_t group_number, int32_t peer_number,
                         const uint8_t *message, uint16_t length, void *data,
                         TOX_MESSAGE_TYPE message_type)
{
    TOX_ERR_CONFERENCE_PEER_QUERY err = TOX_ERR_CONFERENCE_PEER_QUERY_OK;
    bool rc;
    struct t_twc_profile *profile = data;

    struct t_twc_chat *chat =
        twc_chat_search_group(profile, group_number, true);

    char *myname = twc_get_self_name_nt(profile->tox);
    char *name = twc_get_peer_name_nt(profile->tox, group_number, peer_number);
    char *tags = "notify_message";
    char *message_nt = twc_null_terminate(message, length);

    const char *nick_color;
    rc = tox_conference_peer_number_is_ours(tox, group_number, peer_number,
                                            &err);
    if (rc && (err == TOX_ERR_CONFERENCE_PEER_QUERY_OK))
        nick_color = weechat_color("chat_nick_self");
    else
        nick_color = weechat_info_get("nick_color", name);

    if (weechat_string_has_highlight(message_nt, myname))
        tags = "notify_highlight";
    twc_chat_print_message(chat, tags, nick_color, name, message_nt,
                           message_type);

    free(name);
    free(myname);
    free(message_nt);
}

void
twc_group_message_callback(Tox *tox, uint32_t group_number,
                           uint32_t peer_number, TOX_MESSAGE_TYPE type,
                           const uint8_t *message, size_t length, void *data)
{
    twc_handle_group_message(tox, group_number, peer_number, message, length,
                             data, type);
}

void
twc_group_peer_list_changed_callback(Tox *tox, uint32_t group_number,
                                     void *data)
{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat =
        twc_chat_search_group(profile, group_number, true);

    struct t_gui_nick *nick = NULL;
    int i, npeers;
    TOX_ERR_CONFERENCE_PEER_QUERY err = TOX_ERR_CONFERENCE_PEER_QUERY_OK;

    struct t_weelist *new_nicks;
    struct t_weelist_item *n;

    npeers = tox_conference_peer_count(profile->tox, group_number, &err);

    if (err == TOX_ERR_CONFERENCE_PEER_QUERY_OK)
    {
        new_nicks = weechat_list_new();
        for (i = 0; i < npeers; i++)
        {
            char *name = twc_get_peer_name_nt(profile->tox, group_number, i);
            weechat_list_add(new_nicks, name, WEECHAT_LIST_POS_END, NULL);
            free(name);
        }
    }
    else
        return;

    /* searching for exits */
    n = weechat_list_get(chat->nicks, 0);

    while (n)
    {
        const char *name = weechat_list_string(n);
        if (!weechat_list_search(new_nicks, name))
        {
            weechat_printf(chat->buffer, "%s%s just left the group chat",
                           weechat_prefix("quit"), name);
            nick = weechat_nicklist_search_nick(chat->buffer,
                                                chat->nicklist_group, name);
            weechat_nicklist_remove_nick(chat->buffer, nick);
        }
        n = weechat_list_next(n);
    }

    /* searching for joins */
    n = weechat_list_get(new_nicks, 0);

    while (n)
    {
        const char *name = weechat_list_string(n);
        if (!weechat_list_search(chat->nicks, name))
        {
            weechat_printf(chat->buffer, "%s%s just joined the group chat",
                           weechat_prefix("join"), name);
            weechat_nicklist_add_nick(chat->buffer, chat->nicklist_group, name,
                                      NULL, NULL, NULL, 1);
        }
        n = weechat_list_next(n);
    }

    weechat_list_remove_all(chat->nicks);
    weechat_list_free(chat->nicks);
    chat->nicks = new_nicks;
}

void
twc_group_peer_name_callback(Tox *tox, uint32_t group_number,
                             uint32_t peer_number, const uint8_t *pname,
                             size_t pname_len, void *data)
{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat =
        twc_chat_search_group(profile, group_number, true);

    int npeers, len;
    struct t_gui_nick *nick = NULL;
    const char *prev_name;
    char *name;
    bool rc;
    TOX_ERR_CONFERENCE_PEER_QUERY err = TOX_ERR_CONFERENCE_PEER_QUERY_OK;

    struct t_weelist_item *n;

    npeers = tox_conference_peer_count(profile->tox, group_number, &err);

    if (err == TOX_ERR_CONFERENCE_PEER_QUERY_OK)
    {
        len = weechat_list_size(chat->nicks);
        if (len != npeers)
        {
            // We missed some events, fallback to full list update
            twc_group_peer_list_changed_callback(tox, group_number, data);
            return;
        }
    }
    else
        return;

    n = weechat_list_get(chat->nicks, peer_number);
    if (!n)
    {
        // We missed some events, fallback to full list update
        twc_group_peer_list_changed_callback(tox, group_number, data);
        return;
    }

    prev_name = weechat_list_string(n);
    name = twc_null_terminate(pname, pname_len);

    nick = weechat_nicklist_search_nick(chat->buffer, chat->nicklist_group,
                                        prev_name);

    weechat_nicklist_remove_nick(chat->buffer, nick);

    err = TOX_ERR_CONFERENCE_PEER_QUERY_OK;
    rc = tox_conference_peer_number_is_ours(tox, group_number, peer_number,
                                            &err);
    if ((err == TOX_ERR_CONFERENCE_PEER_QUERY_OK) && (!rc))
        weechat_printf(chat->buffer, "%s%s is now known as %s",
                       weechat_prefix("network"), prev_name, name);

    weechat_list_set(n, name);

    weechat_nicklist_add_nick(chat->buffer, chat->nicklist_group, name, NULL,
                              NULL, NULL, 1);

    free(name);
}

void
twc_group_title_callback(Tox *tox, uint32_t group_number, uint32_t peer_number,
                         const uint8_t *title, size_t length, void *data)
{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat =
        twc_chat_search_group(profile, group_number, true);
    twc_chat_queue_refresh(chat);

    char *name = twc_get_peer_name_nt(profile->tox, group_number, peer_number);

    char *topic = strndup((char *)title, length);
    weechat_printf(chat->buffer, "%s%s has changed the topic to \"%s\"",
                   weechat_prefix("network"), name, topic);
    free(topic);
}

void
twc_file_recv_control_callback(Tox *tox, uint32_t friend_number,
                               uint32_t file_number, TOX_FILE_CONTROL control,
                               void *user_data)
{
    struct t_twc_profile *profile = twc_profile_search_tox(tox);
    struct t_twc_tfer_file *file =
        twc_tfer_file_get_by_number(profile->tfer, file_number);
    if (!file)
    {
        weechat_printf(profile->tfer->buffer,
                       "%sthere is no file with number %i in queue",
                       weechat_prefix("error"), file_number);
        return;
    }
    switch (control)
    {
        case TOX_FILE_CONTROL_RESUME:
            TWC_TFER_FILE_UPDATE_STATUS(TWC_TFER_FILE_STATUS_IN_PROGRESS);
            break;
        case TOX_FILE_CONTROL_PAUSE:
            if (file->position != 0)
                TWC_TFER_FILE_UPDATE_STATUS(TWC_TFER_FILE_STATUS_PAUSED);
            break;
        case TOX_FILE_CONTROL_CANCEL:
            fclose(file->fp);
            if (file->type == TWC_TFER_FILE_TYPE_DOWNLOADING &&
                file->size != UINT64_MAX)
                remove(file->full_path);
            if (file->position != 0)
            {
                TWC_TFER_FILE_UPDATE_STATUS(TWC_TFER_FILE_STATUS_ABORTED);
            }
            else
            {
                TWC_TFER_FILE_UPDATE_STATUS(TWC_TFER_FILE_STATUS_DECLINED);
            }
            break;
    }
}

void
twc_file_chunk_request_callback(Tox *tox, uint32_t friend_number,
                                uint32_t file_number, uint64_t position,
                                size_t length, void *user_data)
{
    struct t_twc_profile *profile = twc_profile_search_tox(tox);
    struct t_twc_tfer_file *file =
        twc_tfer_file_get_by_number(profile->tfer, file_number);
    /* the file is missing */
    if (!file)
    {
        weechat_printf(profile->tfer->buffer,
                       "%sthere is no file with number %i in queue",
                       weechat_prefix("error"), file_number);
        return;
    }
    /* 0-length chunk requested that means the file transmission is completed */
    if (length == 0)
    {
        TWC_TFER_FILE_UPDATE_STATUS(TWC_TFER_FILE_STATUS_DONE);

        /* This friend_number will be re-used and re-assigned for another file,
         * to prevent collisions in twc_tfer_file_get_by_number calls let's
         * set it to a value that won't be used by toxcore.
         */
        file->file_number = UINT32_MAX;
        fclose(file->fp);
        return;
    }
    uint8_t *data = twc_tfer_file_get_chunk(file, position, length);
    if (!data)
    {
        weechat_printf(profile->buffer, "%serror while reading the file %s",
                       weechat_prefix("error"), file->filename);
        return;
    }
    enum TOX_ERR_FILE_SEND_CHUNK error;
    tox_file_send_chunk(profile->tox, friend_number, file_number, position,
                        data, length, &error);
    if (error)
        weechat_printf(profile->buffer, "%s%s: chunk sending error: %s",
                       weechat_prefix("error"), file->filename,
                       twc_tox_err_file_send_chunk(error));
    else
    {
        file->position += length;
        file->after_last_cache += length;
        TWC_TFER_FILE_UPDATE_STATUS(TWC_TFER_FILE_STATUS_IN_PROGRESS);
        if ((twc_tfer_get_time() - file->timestamp) > 1)
        {
            file->timestamp = twc_tfer_get_time();
            file->after_last_cache = 0;
        }
    }
    free(data);
}

void
twc_file_recv_callback(Tox *tox, uint32_t friend_number, uint32_t file_number,
                       uint32_t kind, uint64_t file_size,
                       const uint8_t *filename, size_t filename_length,
                       void *user_data)
{
    struct t_twc_profile *profile = twc_profile_search_tox(tox);
    if (kind == TOX_FILE_KIND_AVATAR)
    {
        TOX_ERR_FILE_CONTROL error;
        tox_file_control(tox, friend_number, file_number,
                         TOX_FILE_CONTROL_CANCEL, &error);
        if (error)
        {
            weechat_printf(profile->buffer, "%scannot cancel avatar receiving",
                           weechat_prefix("error"));
        }
        return;
    }
    char *name = twc_get_name_nt(tox, friend_number);
    char *fname = twc_null_terminate(filename, filename_length);
    struct t_twc_tfer_file *file =
        twc_tfer_file_new(profile, name, fname, friend_number, file_number,
                          file_size, TWC_TFER_FILE_TYPE_DOWNLOADING);
    free(name);
    free(fname);
    if (!file)
    {
        weechat_printf(profile->buffer,
                       "%scannot open the file \"%s\" with write permissions",
                       weechat_prefix("error"), filename);
        return;
    }
    if (!(profile->tfer->buffer))
    {
        twc_tfer_load(profile);
    }
    twc_tfer_file_add(profile->tfer, file);
    twc_tfer_file_update(profile->tfer, file);
    twc_tfer_update_status(profile->tfer, "waiting for action");
}

void
twc_file_recv_chunk_callback(Tox *tox, uint32_t friend_number,
                             uint32_t file_number, uint64_t position,
                             const uint8_t *data, size_t length,
                             void *user_data)
{
    struct t_twc_profile *profile = twc_profile_search_tox(tox);
    struct t_twc_tfer_file *file =
        twc_tfer_file_get_by_number(profile->tfer, file_number);
    /* the file is missing */
    if (!file)
    {
        weechat_printf(profile->tfer->buffer,
                       "%sthere is no file with number %i in queue",
                       weechat_prefix("error"), file_number);
        return;
    }
    /* 0-length chunk transmitted that means the file transmission is completed
     */
    if (length == 0)
    {
        TWC_TFER_FILE_UPDATE_STATUS(TWC_TFER_FILE_STATUS_DONE);

        /* This friend_number will be re-used and re-assigned for another file,
         * to prevent collisions in twc_tfer_file_get_by_number calls let's
         * set it to a value that won't be used by toxcore.
         */
        file->file_number = UINT32_MAX;
        fclose(file->fp);
        return;
    }
    bool result = twc_tfer_file_write_chunk(file, data, position, length);
    if (!result)
    {
        weechat_printf(profile->buffer, "%serror while writing the file %s",
                       weechat_prefix("error"), file->filename);
        return;
    }
    else
    {
        file->position += length;
        file->after_last_cache += length;
        twc_tfer_file_update(profile->tfer, file);
        if ((twc_tfer_get_time() - file->timestamp) > 1)
        {
            file->timestamp = twc_tfer_get_time();
            file->after_last_cache = 0;
        }
    }
}

#ifndef NDEBUG
void
twc_tox_log_callback(Tox *tox, TOX_LOG_LEVEL level, const char *file,
                     uint32_t line, const char *func, const char *message,
                     void *user_data)
{
    struct t_twc_profile *const profile = user_data;

    char const *color;
    switch (level)
    {
        case TOX_LOG_LEVEL_TRACE:
            color = weechat_color("gray");
            break;
        case TOX_LOG_LEVEL_DEBUG:
            color = weechat_color("white");
            break;
        case TOX_LOG_LEVEL_INFO:
            color = weechat_color("lightblue");
            break;
        case TOX_LOG_LEVEL_WARNING:
            color = weechat_color("yellow");
            break;
        case TOX_LOG_LEVEL_ERROR:
            color = weechat_color("red");
            break;
        default:
            color = weechat_color("reset");
    }

    weechat_printf(profile->buffer, "%stox\t%s%s:%" PRIu32 " [%s]%s %s", color,
                   weechat_color("reset"), file, line, func,
                   weechat_color("lightred"), message);
}
#endif /* !NDEBUG */
