/*
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

#ifndef TOX_WEECHAT_TFER_H
#define TOX_WEECHAT_TFER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <tox/tox.h>
#include <weechat/weechat-plugin.h>

#include "twc-list.h"
#include "twc-profile.h"

#define TWC_TFER_LEGEND_LINES (4)
#define TWC_TFER_FILE_STATUS_MAX_LENGTH (256)
#define TWC_MAX_CHUNK_LENGTH (1371)
#define TWC_MAX_SIZE_SUFFIX (5)
#define TWC_MAX_SPEED_SUFFIX (5)

enum t_twc_tfer_file_status
{
    TWC_TFER_FILE_STATUS_REQUEST,
    TWC_TFER_FILE_STATUS_IN_PROGRESS,
    TWC_TFER_FILE_STATUS_PAUSED,
    TWC_TFER_FILE_STATUS_DONE,
    TWC_TFER_FILE_STATUS_DECLINED,
    TWC_TFER_FILE_STATUS_ABORTED,
};

enum t_twc_tfer_file_type
{
    TWC_TFER_FILE_TYPE_DOWNLOADING,
    TWC_TFER_FILE_TYPE_UPLOADING,
};

struct t_twc_tfer_file
{
    enum t_twc_tfer_file_status status;
    enum t_twc_tfer_file_type type;
    uint64_t position; /* already transmitted (in bytes) */
    uint64_t size;
    uint32_t friend_number;
    uint32_t file_number;
    FILE *fp;
    char *filename;
    char *full_path;
    char *nickname;
    double timestamp;
    float cached_speed;
    size_t after_last_cache;
};

struct t_twc_tfer
{
    struct t_twc_list *files;
    struct t_gui_buffer *buffer;
    char *downloading_path;
};

int
twc_tfer_buffer_input_callback(const void *pointer, void *data,
                               struct t_gui_buffer *weechat_buffer,
                               const char *input_data);
int
twc_tfer_buffer_close_callback(const void *pointer, void *data,
                               struct t_gui_buffer *weechat_buffer);
struct t_twc_tfer *
twc_tfer_new();

enum t_twc_rc
twc_tfer_load(struct t_twc_profile *profile);

bool
twc_tfer_has_buffer(struct t_twc_profile *profile);

int
twc_tfer_print_legend(struct t_twc_tfer *tfer);

double
twc_tfer_get_time();

void
twc_tfer_update_downloading_path(struct t_twc_profile *profile);

char *
twc_tfer_file_name_strip(const char *original, size_t size);

struct t_twc_tfer_file *
twc_tfer_file_new(struct t_twc_profile *profile, const char *nickname,
                  const char *filename, uint32_t friend_number,
                  uint32_t file_number, uint64_t size,
                  enum t_twc_tfer_file_type filetype);

void
twc_tfer_file_add(struct t_twc_tfer *tfer, struct t_twc_tfer_file *file);

uint8_t *
twc_tfer_file_get_chunk(struct t_twc_tfer_file *file, uint64_t position,
                        size_t length);

bool
twc_tfer_file_write_chunk(struct t_twc_tfer_file *file, const uint8_t *data,
                          uint64_t position, size_t length);

struct t_twc_tfer_file *
twc_tfer_file_get_by_number(struct t_twc_tfer *tfer, uint32_t file_number);

size_t
twc_tfer_file_get_index(struct t_twc_tfer *tfer, struct t_twc_tfer_file *file);

void
twc_tfer_file_update(struct t_twc_tfer *tfer, struct t_twc_tfer_file *file);

int
twc_tfer_file_accept(struct t_twc_profile *profile, size_t index);

int
twc_tfer_file_decline(struct t_twc_profile *profile, size_t index);

int
twc_tfer_file_pause(struct t_twc_profile *profile, size_t index);

int
twc_tfer_file_continue(struct t_twc_profile *profile, size_t index);

int
twc_tfer_file_abort(struct t_twc_profile *profile, size_t index);

int
twc_tfer_update_status(struct t_twc_tfer *tfer, const char *status);

void
twc_tfer_buffer_update(struct t_twc_tfer *tfer);

void
twc_tfer_file_err_send_message(char *message, enum TOX_ERR_FILE_SEND error);

void
twc_tfer_file_free(struct t_twc_tfer_file *file);

void
twc_tfer_free(struct t_twc_tfer *tfer);

#endif /* TOX_WEECHAT_TFER_H */
