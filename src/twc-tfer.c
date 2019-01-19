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

#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <tox/tox.h>
#include <weechat/weechat-plugin.h>

#include "twc-list.h"
#include "twc-profile.h"
#include "twc-tfer.h"
#include "twc-utils.h"
#include "twc.h"

#define PROGRESS_BAR_LEN (50)

#define TWC_TFER_UPDATE_STATUS_AND_RETURN(fmt, ...)                            \
    do                                                                         \
    {                                                                          \
        sprintf(status, fmt, ##__VA_ARGS__);                                   \
        twc_tfer_update_status(profile->tfer, status);                         \
        weechat_string_free_split(argv);                                       \
        free(status);                                                          \
        return WEECHAT_RC_OK;                                                  \
    } while (0)

#define TWC_TFER_MESSAGE(present, past)                                        \
    do                                                                         \
    {                                                                          \
        int result = twc_tfer_file_##present(profile, n);                      \
        switch (result)                                                        \
        {                                                                      \
            case 1:                                                            \
                TWC_TFER_UPDATE_STATUS_AND_RETURN(                             \
                    "request number %zu has been " #past, n);                  \
            case 0:                                                            \
                TWC_TFER_UPDATE_STATUS_AND_RETURN(                             \
                    "request number %zu cannot be " #past " because "          \
                    "of tox internal issues",                                  \
                    n);                                                        \
            case -1:                                                           \
                TWC_TFER_UPDATE_STATUS_AND_RETURN(                             \
                    "request number %zu cannot be " #past, n);                 \
        }                                                                      \
    } while (0)

/**
 * Create a new "tfer" object that handles a list of transmitting files and
 * a buffer for managing them.
 */
struct t_twc_tfer *
twc_tfer_new()
{
    struct t_twc_tfer *tfer = malloc(sizeof(struct t_twc_tfer));
    tfer->files = twc_list_new();
    tfer->buffer = NULL;
    tfer->downloading_path = NULL;
    return tfer;
}

/**
 * Load "tfer" buffer and make it ready for usage.
 */
enum t_twc_rc
twc_tfer_load(struct t_twc_profile *profile)
{
    /* create "tfer" buffer */
    struct t_gui_buffer *buffer;
    char *name = malloc(sizeof(profile->name) + 5);
    sprintf(name, "tfer/%s", profile->name);
    profile->tfer->buffer = buffer = weechat_buffer_new(
        name, twc_tfer_buffer_input_callback, (void *)profile, NULL,
        twc_tfer_buffer_close_callback, (void *)profile, NULL);
    free(name);
    if (!buffer)
        return TWC_RC_ERROR;
    /* set all parameters of the buffer*/
    weechat_buffer_set(buffer, "type", "free");
    weechat_buffer_set(buffer, "notify", "1");
    weechat_buffer_set(buffer, "day_change", "0");
    weechat_buffer_set(buffer, "clear", "0");
    weechat_buffer_set(buffer, "nicklist", "0");
    weechat_buffer_set(buffer, "title", "File transmission");
    twc_tfer_print_legend(profile->tfer);
    return TWC_RC_OK;
}

/**
 * Display status line and available commands.
 */
int
twc_tfer_print_legend(struct t_twc_tfer *tfer)
{
    char *text[TWC_TFER_LEGEND_LINES] = {
        "status: OK", /* This line is reserved for the status */
        "r: refresh   | a <n>: accept   | d <n>: decline",
        "p <n>: pause | c <n>: continue | b <n>: abort", "files:"};
    int i;
    for (i = 0; i < TWC_TFER_LEGEND_LINES; i++)
    {
        weechat_printf_y(tfer->buffer, i, "%s", text[i]);
    }
    return WEECHAT_RC_OK;
}

/**
 * Expand %h and %p in path.
 * Returned string must be freed.
 */
char *
twc_tfer_expanded_path(struct t_twc_profile *profile, const char *base_path)
{
    const char *weechat_dir = weechat_info_get("weechat_dir", NULL);
    char *home_expanded = weechat_string_replace(base_path, "%h", weechat_dir);
    char *full_path =
        weechat_string_replace(home_expanded, "%p", profile->name);
    free(home_expanded);
    return full_path;
}

/**
 * Set profile-associated path for downloads.
 * If it is impossible to create a directory with the path that
 * has been set in tox.profile.<name>.downloading_path then default
 * value will be used.
 */
void
twc_tfer_update_downloading_path(struct t_twc_profile *profile)
{
    const char *base_path =
        TWC_PROFILE_OPTION_STRING(profile, TWC_PROFILE_OPTION_DOWNLOADING_PATH);
    char *full_path = twc_tfer_expanded_path(profile, base_path);
    if (!weechat_mkdir_parents(full_path, 0755))
    {
        char *bad_path = full_path;
        base_path = weechat_config_string(
            twc_config_profile_default[TWC_PROFILE_OPTION_DOWNLOADING_PATH]);
        full_path = twc_tfer_expanded_path(profile, base_path);
        weechat_printf(profile->buffer,
                       "cannot create directory \"%s\","
                       "using default value: \"%s\"",
                       bad_path, full_path);
        free(bad_path);
        weechat_mkdir_parents(full_path, 0755);
    }
    free(profile->tfer->downloading_path);
    profile->tfer->downloading_path = full_path;
}

/**
 * Check if there's an access to the file.
 */
bool
twc_tfer_file_check(const char *filename)
{
    return access(filename, F_OK) == -1 ? false : true;
}

/**
 * Add "*(<number>).*" to the filename.
 * Returns a pointer to allocated string and must be freed after use.
 */
char *
twc_tfer_file_unique_name(const char *original)
{
    char *name = malloc(sizeof(char) * (FILENAME_MAX + 1));
    name[FILENAME_MAX] = '\0';
    /* in case if someone sent way too long filename */
    strncpy(name, original, FILENAME_MAX);
    if (!twc_tfer_file_check(name))
        return name;
    /* a file with the given name is already exist */
    int i;
    char *extension;
    char *dot;
    if ((dot = strrchr(original, '.')))
    {
        name[dot - original] = '\0';
        extension = dot;
    }
    else
        extension = "";
    char body[strlen(name) + 1];
    strcpy(body, name);

    /* check if there is already a postfix number in the end of the file
     *  surrounded by parenthesis */
    char *left, *right;
    long number = 1; /* number postfix */
    if ((left = strrchr(body, '(')) && (right = strrchr(body, ')')))
    {
        if (!(number = strtol(left + sizeof(char), NULL, 0)) &&
            *(right - sizeof(char)) != '0')
        {
            number = 1;
        }
        else
        {
            *left = '\0';
        }
    }
    /* trying names until success */
    i = number;
    do
    {
        snprintf(name, FILENAME_MAX, "%s(%d)%s", body, i, extension);
        i++;
    } while (twc_tfer_file_check(name));

    return name;
}

/**
 * Delete everything before "/" (including "/") from the filename.
 * Returns a pointer to allocated string and must be freed after use.
 */
char *
twc_tfer_file_name_strip(const char *original, size_t size)
{
    char *name = malloc(sizeof(char) * size);
    char *slash, *offset = (char *)original;
    if ((slash = strrchr(original, '/')))
        offset = slash + sizeof(char);
    if (strlen(offset))
    {
        sprintf(name, "%s", offset);
        return name;
    }
    else
        return NULL;
}

/**
 * Create a new file.
 */
struct t_twc_tfer_file *
twc_tfer_file_new(struct t_twc_profile *profile, const char *nickname,
                  const char *filename, uint32_t friend_number,
                  uint32_t file_number, uint64_t size,
                  enum t_twc_tfer_file_type filetype)
{
    struct t_twc_tfer_file *file = malloc(sizeof(struct t_twc_tfer_file));
    file->status = TWC_TFER_FILE_STATUS_REQUEST;
    file->type = filetype;
    file->position = 0;
    file->timestamp = 0;
    file->cached_speed = 0;
    file->after_last_cache = 0;
    file->nickname = strdup(nickname);
    file->friend_number = friend_number;
    file->file_number = file_number;
    file->size = size;
    if (filetype == TWC_TFER_FILE_TYPE_DOWNLOADING)
    {
        char *full_path = malloc(sizeof(char) * (FILENAME_MAX + 1));
        sprintf(full_path, "%s", profile->tfer->downloading_path);
        char *final_name = twc_tfer_file_name_strip(
            filename, FILENAME_MAX + 1 - strlen(full_path));
        if (!final_name)
            return NULL;

        char *slash = strrchr(full_path, '/');
        if (*(slash + sizeof(char)) != '\0')
            strcat(full_path, "/");

        strcat(full_path, final_name);
        char *final_path = twc_tfer_file_unique_name(full_path);

        file->filename = strdup(strrchr(final_path, '/') + sizeof(char));
        file->full_path = final_path;

        file->fp = fopen(final_path, "w");

        free(final_name);
        free(full_path);
    }
    else
    {
        file->filename = twc_tfer_file_name_strip(
            filename, FILENAME_MAX + 1 - strlen(filename));
        file->full_path = NULL;
        file->fp = fopen(filename, "r");
    }

    if (!(file->fp))
        return NULL;
    return file;
}

/**
 * Add file to the buffer
 */
void
twc_tfer_file_add(struct t_twc_tfer *tfer, struct t_twc_tfer_file *file)
{
    twc_list_item_new_data_add(tfer->files, file);
}

/**
 * Get file type: "<=" (downloading) or "=>" (uploading).
 */
const char *
twc_tfer_file_get_type_str(struct t_twc_tfer_file *file)
{
    return file->type == TWC_TFER_FILE_TYPE_DOWNLOADING ? "<=" : "=>";
}

/**
 * Get file status string (excluding TWC_TFER_FILE_STATUS_IN_PROGRESS).
 */
const char *
twc_tfer_file_get_status_str(struct t_twc_tfer_file *file)
{
    char *statuses[] = {"[request]", "",           "[paused]",
                        "[done]",    "[declined]", "[aborted]"};
    return statuses[file->status];
}

/**
 * Get cut file size.
 */
float
twc_tfer_cut_size(size_t size)
{
    float ret = size;
    int i = 0;
    while ((ret > 1024) && (i < TWC_MAX_SIZE_SUFFIX))
    {
        ret /= 1024.0;
        i++;
    }
    return ret;
}

/**
 * Get file size suffix.
 */
const char *
twc_tfer_size_suffix(uint64_t size)
{
    char *suffixes[] = {"", "K", "M", "G", "T"};
    uint64_t ret = size;
    int i = 0;
    while ((ret > 1024) && (i < TWC_MAX_SIZE_SUFFIX))
    {
        ret /= 1024.0;
        i++;
    }
    return suffixes[i];
}

/**
 * Get cut speed value.
 */
float
twc_tfer_cut_speed(float speed)
{
    float ret = speed;
    int i = 0;
    while ((ret > 1024) && (i < TWC_MAX_SPEED_SUFFIX))
    {
        ret /= 1024.0;
        i++;
    }
    return ret;
}

/**
 * Get speed suffix.
 */
const char *
twc_tfer_speed_suffix(float speed)
{
    char *suffixes[] = {"bytes/s", "KB/s", "MB/s", "GB/s", "TB/s"};
    uint64_t ret = speed;
    int i = 0;
    while ((ret > 1024) && (i < TWC_MAX_SPEED_SUFFIX))
    {
        ret /= 1024.0;
        i++;
    }
    return suffixes[i];
}

/**
 * Get current time with nanoseconds since the Epoch.
 */
double
twc_tfer_get_time()
{
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    return (double)tp.tv_sec + (double)(tp.tv_nsec / 1E9);
}

/**
 * Get speed of transmission.
 */
float
twc_tfer_get_speed(struct t_twc_tfer_file *file)
{
    if (file->timestamp == 0)
        return 0;
    double diff = twc_tfer_get_time() - file->timestamp;
    if (diff < 1)
        return file->cached_speed;
    float result = (float)(file->after_last_cache) / (diff * diff);
    file->cached_speed = result;
    return result;
}

/**
 * Update buffer strings for a certain file.
 */
void
twc_tfer_file_update(struct t_twc_tfer *tfer, struct t_twc_tfer_file *file)
{
    size_t index = twc_tfer_file_get_index(tfer, file);
    size_t line = index * 2 + TWC_TFER_LEGEND_LINES;
    const char *type = twc_tfer_file_get_type_str(file);
    size_t indent = 0;
    size_t remainder = index;
    do
    {
        remainder = remainder / 10;
        indent++;
    } while (remainder > 0);
    indent += 5; /* length of ") => " */
    char placeholder[indent + 1];
    memset(placeholder, ' ', indent);
    placeholder[indent] = '\0';
    const char *status = twc_tfer_file_get_status_str(file);
    if (file->size == UINT64_MAX)
    {
        weechat_printf_y(tfer->buffer, line, "%zu) %s %s: %s [STREAM]", index,
                         type, file->nickname, file->filename);
    }
    else
    {
        float display_size = twc_tfer_cut_size(file->size);
        const char *size_suffix = twc_tfer_size_suffix(file->size);
        weechat_printf_y(tfer->buffer, line, "%zu) %s %s: %s %zu (%.2f%s)", index,
                         type, file->nickname, file->filename, file->size,
                         display_size, size_suffix);
    }
    if (file->status == TWC_TFER_FILE_STATUS_IN_PROGRESS)
    {
        float speed = twc_tfer_get_speed(file);
        float display_speed = twc_tfer_cut_speed(speed);
        const char *speed_suffix = twc_tfer_speed_suffix(speed);
        if (file->size == UINT64_MAX)
        {
            weechat_printf_y(tfer->buffer, line + 1, "%s%.2f%s", placeholder,
                             display_speed, speed_suffix);
            return;
        }
        double ratio = (double)(file->position) / (double)(file->size);
        int percents = (int)(ratio * 100);

        char progress_bar[PROGRESS_BAR_LEN + 1];
        memset(progress_bar, ' ', PROGRESS_BAR_LEN);
        int i;
        for (i = 0; i < PROGRESS_BAR_LEN * ratio; i++)
            progress_bar[i] = '=';
        if (i < PROGRESS_BAR_LEN)
            progress_bar[i] = '>';
        progress_bar[PROGRESS_BAR_LEN] = '\0';

        float display_pos = twc_tfer_cut_size(file->position);
        const char *pos_suffix = twc_tfer_size_suffix(file->position);

        weechat_printf_y(tfer->buffer, line + 1, "%s%d%% [%s] %.2f%s %.2f%s",
                         placeholder, percents, progress_bar, display_pos,
                         pos_suffix, display_speed, speed_suffix);
    }
    else
        weechat_printf_y(tfer->buffer, line + 1, "%s%s", placeholder, status);
}

/**
 * Allocate and return "uint8_t data[length]" chunk of data starting from
 * "position".
 */
uint8_t *
twc_tfer_file_get_chunk(struct t_twc_tfer_file *file, uint64_t position,
                        size_t length)
{
    fseek(file->fp, position, SEEK_SET);
    uint8_t *data = malloc(sizeof(uint8_t) * length);
    size_t read = fread(data, sizeof(uint8_t), length, file->fp);
    while ((read < length) && !feof(file->fp))
    {
        read += fread(data + read * sizeof(uint8_t), sizeof(uint8_t),
                      length - read, file->fp);
    }
    if (read != length)
        return NULL;
    return data;
}

/**
 * Write a chunk to the file.
 */
bool
twc_tfer_file_write_chunk(struct t_twc_tfer_file *file, const uint8_t *data,
                          uint64_t position, size_t length)
{
    fseek(file->fp, position, SEEK_SET);
    size_t wrote = fwrite(data, sizeof(uint8_t), length, file->fp);
    while (wrote < length)
    {
        wrote += fwrite(data + wrote * sizeof(uint8_t), sizeof(uint8_t),
                        length - wrote, file->fp);
    }

    if (wrote != length)
        return false;
    return true;
}

/**
 * Return file by its "uint32_t file_number".
 */
struct t_twc_tfer_file *
twc_tfer_file_get_by_number(struct t_twc_tfer *tfer, uint32_t file_number)
{
    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach (tfer->files, index, item)
    {
        if (item->file->file_number == file_number)
            return item->file;
    }
    return NULL;
}

/**
 * Return file index.
 */
size_t
twc_tfer_file_get_index(struct t_twc_tfer *tfer, struct t_twc_tfer_file *file)
{
    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach (tfer->files, index, item)
    {
        if (item->file == file)
            return index;
    }
    return SIZE_MAX;
}

/**
 * Update status line of the "tfer" buffer.
 */
int
twc_tfer_update_status(struct t_twc_tfer *tfer, const char *status)
{
    weechat_printf_y(tfer->buffer, 0, "status: %s", status);
    weechat_buffer_set(tfer->buffer, "hotlist", WEECHAT_HOTLIST_HIGHLIGHT);
    return WEECHAT_RC_OK;
}

/**
 * Update "tfer" buffer
 */
void
twc_tfer_buffer_update(struct t_twc_tfer *tfer)
{
    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach (tfer->files, index, item)
    {
        twc_tfer_file_update(tfer, item->file);
    }
}

/**
 * Refresh the entire buffer, i.e. delete files with status of:
 * [denied], [aborted], [done].
 */
void
twc_tfer_buffer_refresh(struct t_twc_tfer *tfer)
{
    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach (tfer->files, index, item)
    {
        enum t_twc_tfer_file_status status = item->file->status;
        if (status == TWC_TFER_FILE_STATUS_DECLINED ||
            status == TWC_TFER_FILE_STATUS_ABORTED ||
            status == TWC_TFER_FILE_STATUS_DONE)
        {
            struct t_twc_tfer_file *file = twc_list_remove(item);
            twc_tfer_file_free(file);
        }
    }
    weechat_buffer_clear(tfer->buffer);
    twc_tfer_print_legend(tfer);
    twc_tfer_buffer_update(tfer);
}

/**
 * Send TOX_FILE_CONTROL command to a client.
 * "сheck" is a file status that a file should be in before sending a control
 * command. "send" is a control comand you are going to send. "set" is a file
 * status that will be set after successful sending a control command.
 */
int
twc_tfer_file_send_control(struct t_twc_profile *profile, size_t index,
                           enum t_twc_tfer_file_status check,
                           enum TOX_FILE_CONTROL send,
                           enum t_twc_tfer_file_status set)
{
    struct t_twc_tfer_file *file;
    struct t_twc_list_item *item = twc_list_get(profile->tfer->files, index);
    file = item->file;
    if (file->status != check)
        return -1;
    if (file->type == TWC_TFER_FILE_TYPE_UPLOADING &&
        send == TOX_FILE_CONTROL_RESUME)
        return -1;
    enum TOX_ERR_FILE_CONTROL control_error;
    tox_file_control(profile->tox, file->friend_number, file->file_number, send,
                     &control_error);
    if (control_error)
    {
        weechat_printf(profile->buffer,
                       "%scannot send control command for \"%s\" file: %s",
                       weechat_prefix("error"), file->filename,
                       twc_tox_err_file_control(control_error));
        return 0;
    }
    else
    {
        if (send == TOX_FILE_CONTROL_CANCEL)
        {
            fclose(file->fp);
            if (file->type == TWC_TFER_FILE_TYPE_DOWNLOADING &&
                file->size != UINT64_MAX)
                remove(file->full_path);
        }
        file->status = set;
        twc_tfer_file_update(profile->tfer, file);
        return 1;
    }
}

/**
 * Accept a file with number <index> in the list.
 * Returns 1 if successful, 0 when there's an issue with tox calls
 * and -1 if the request is already accepted or declined.
 */
int
twc_tfer_file_accept(struct t_twc_profile *profile, size_t index)
{
    return twc_tfer_file_send_control(
        profile, index, TWC_TFER_FILE_STATUS_REQUEST, TOX_FILE_CONTROL_RESUME,
        TWC_TFER_FILE_STATUS_IN_PROGRESS);
}

/**
 * Decline a file with number <index> in the list.
 * Returns 1 if successful, 0 when there's an issue with tox calls
 * and -1 if the request is already accepted or declined.
 */
int
twc_tfer_file_decline(struct t_twc_profile *profile, size_t index)
{
    return twc_tfer_file_send_control(
        profile, index, TWC_TFER_FILE_STATUS_REQUEST, TOX_FILE_CONTROL_CANCEL,
        TWC_TFER_FILE_STATUS_DECLINED);
}

/**
 * Pause transmission of the file with number <index> in the list.
 * Returns 1 if successful, 0 when there's an issue with tox calls
 * and -1 if transmission is already paused.
 */
int
twc_tfer_file_pause(struct t_twc_profile *profile, size_t index)
{
    return twc_tfer_file_send_control(
        profile, index, TWC_TFER_FILE_STATUS_IN_PROGRESS,
        TOX_FILE_CONTROL_PAUSE, TWC_TFER_FILE_STATUS_PAUSED);
}

/**
 * Continue transmission of the file with number <index> in the list.
 * Returns 1 if successful, 0 when there's an issue with tox calls
 * and -1 if transmission is already going on.
 */
int
twc_tfer_file_continue(struct t_twc_profile *profile, size_t index)
{
    return twc_tfer_file_send_control(
        profile, index, TWC_TFER_FILE_STATUS_PAUSED, TOX_FILE_CONTROL_RESUME,
        TWC_TFER_FILE_STATUS_IN_PROGRESS);
}

/**
 * Abort transmission of the file with number <index> in the list.
 * Returns 1 if successful, 0 when there's an issue with tox calls
 * and -1 if transmission is already aborted.
 */
int
twc_tfer_file_abort(struct t_twc_profile *profile, size_t index)
{
    return twc_tfer_file_send_control(
        profile, index, TWC_TFER_FILE_STATUS_IN_PROGRESS,
        TOX_FILE_CONTROL_CANCEL, TWC_TFER_FILE_STATUS_ABORTED);
}

/**
 * Called when input text is entered on buffer.
 */
int
twc_tfer_buffer_input_callback(const void *pointer, void *data,
                               struct t_gui_buffer *buffer,
                               const char *input_data)
{
    struct t_twc_profile *profile;
    profile = (struct t_twc_profile *)pointer;
    int argc;
    char **argv = weechat_string_split_shell(input_data, &argc);
    char *status =
        malloc(sizeof(char) * weechat_window_get_integer(
                                  weechat_current_window(), "win_width") +
               1);

    /* refresh file list, i.e delete files that have been marked as "denied",
     * "aborted" and "done" */
    if (weechat_strcasecmp(argv[0], "r") == 0)
    {
        if (argc == 1)
        {
            twc_tfer_buffer_refresh(profile->tfer);
            TWC_TFER_UPDATE_STATUS_AND_RETURN("refreshed");
        }
        else
        {
            TWC_TFER_UPDATE_STATUS_AND_RETURN(
                "this command doesn't accept any arguments");
        }
    }
    if (strstr("adpcbADPCB", argv[0]) && argc < 2)
        TWC_TFER_UPDATE_STATUS_AND_RETURN("too few arguments");
    if (argc == 2)
    {
        size_t n = (size_t)strtol(argv[1], NULL, 0);
        if ((n == 0 && strcmp(argv[1], "0") != 0) ||
            n > (profile->tfer->files->count - 1))
        {
            TWC_TFER_UPDATE_STATUS_AND_RETURN(
                "<n> must be existing number of file");
        }
        /* accept */
        if (weechat_strcasecmp(argv[0], "a") == 0)
        {
            TWC_TFER_MESSAGE(accept, accepted);
        }
        /* decline */
        if (weechat_strcasecmp(argv[0], "d") == 0)
        {
            TWC_TFER_MESSAGE(decline, declined);
        }
        /* pause */
        if (weechat_strcasecmp(argv[0], "p") == 0)
        {
            TWC_TFER_MESSAGE(pause, paused);
        }
        /* continue */
        if (weechat_strcasecmp(argv[0], "c") == 0)
        {
            TWC_TFER_MESSAGE(continue, continued);
        }
        /* abort */
        if (weechat_strcasecmp(argv[0], "b") == 0)
        {
            TWC_TFER_MESSAGE(abort, aborted);
        }
    }
    if (argc > 2)
    {
        TWC_TFER_UPDATE_STATUS_AND_RETURN("too many arguments");
    }
    TWC_TFER_UPDATE_STATUS_AND_RETURN("unknown command: %s", argv[0]);
}

/**
 * Called when buffer is closed.
 */
int
twc_tfer_buffer_close_callback(const void *pointer, void *data,
                               struct t_gui_buffer *buffer)
{
    struct t_twc_profile *profile = (struct t_twc_profile *)pointer;
    profile->tfer->buffer = NULL;
    return WEECHAT_RC_OK;
}

void
twc_tfer_file_free(struct t_twc_tfer_file *file)
{
    free(file->filename);
    free(file->nickname);
    if (file->full_path)
        free(file->full_path);
    free(file);
}

void
twc_tfer_free(struct t_twc_tfer *tfer)
{
    struct t_twc_tfer_file *file;
    while ((file = twc_list_pop(tfer->files)))
    {
        twc_tfer_file_free(file);
    }
    free(tfer->files);
    free(tfer->downloading_path);
    free(tfer);
}
