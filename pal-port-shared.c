
#include "cwalk.h"
#include "pal-port.h"

#include <fcntl.h>

void pal_process_info_initialize(pal_process_info_t *info, uint32_t argc, char **argv, uint32_t envc, char **envp)
{
    info->argc = argc;
    info->argv = argv;
    if (envc == -1) {
        int idx;
        for (idx = 0; envp[idx] != NULL; idx++) {
        }
        envc = idx;
    }
    info->envc = envc;
    info->envp = envp;
}

pal_process_info_t *pal_process_info_get()
{
    pal_header_t *pal_header = (pal_header_t *)pal_global();
    return pal_header->info;
}

void pal_process_info_set(pal_process_info_t *new_info)
{
    pal_header_t *pal_header = (pal_header_t *)pal_global();
    if (pal_header->info) {
        pal_free(pal_header->info);
    }
    pal_header->info = new_info;
}

char *pal_session_getcwd(pal_session_t *pal)
{
    pal_header_t *pal_header = (pal_header_t *)pal;
    return pal_header->cwd;
}

int pal_session_chdir(pal_session_t *pal, const char *dirpath)
{
    pal_header_t *pal_header = (pal_header_t *)pal;
    uint32_t dir_len = (uint32_t)strlen(dirpath);
    int ret = -1;
    if (dir_len >= pal_header->cwd_capacity) {
        uint32_t new_capacity = dir_len + 1 + pal_header->cwd_capacity;
        pal_header->cwd = pal_realloc(pal_header->cwd, new_capacity);
        if (pal_header->cwd) {
            pal_header->cwd_capacity = new_capacity;
            pal_header->cwd_length = dir_len;
        } else {
            pal_header->cwd_length = 0;
            pal_header->cwd_capacity = 0;
        }
    }
    if (pal_header->cwd) {
        memcpy(pal_header->cwd, dirpath, dir_len + 1);
        ret = 0;
    } else {
        errno = ENOMEM;
    }
    return ret;
}

int pal_joinpath(int is_absolute, const char *base_path, const char *from_path, char **resolved_path, int offset, int *capacity)
{
    int base_path_len = (int)strlen(base_path);
    if (offset >= *capacity) {
        *capacity += offset;
    }
    if (*resolved_path == NULL && *capacity > 0) {
        *resolved_path = pal_malloc(*capacity);
    }
    for (;;) {
        int len;
        if (is_absolute) {
            len = (int)cwk_path_get_absolute(base_path, from_path, (*resolved_path + offset), *capacity - offset);
        } else {
            len = (int)cwk_path_join(base_path, from_path, (*resolved_path + offset), *capacity - offset);
        }
        if ((offset + len) < *capacity) {
            return len;
        }
        *capacity = offset + len + 1;
        if (base_path == *resolved_path) {
            (*resolved_path)[base_path_len] = '\0';
            *resolved_path = pal_realloc(*resolved_path, *capacity);
            base_path = *resolved_path;
        } else {
            *resolved_path = pal_realloc(*resolved_path, *capacity);
        }
        if (*resolved_path == NULL) {
            errno = ENOMEM;
            return -1;
        }
    }
}

char *pal_session_realpath(pal_session_t *pal, const char *path)
{
    char *out_path = NULL;
    int out_capacity = 0;
    pal_joinpath(1, pal_session_getcwd(pal), path, &out_path, 0, &out_capacity);
    return out_path;
}

static int pal__fs_scandir_filter(const struct pal_dirent_t *dent)
{
    const char *d_name = dent->d_name;
    return strcmp(d_name, ".") != 0 && strcmp(d_name, "..") != 0;
}

int pal_listdir_recurse(
    int is_absolute,
    void *context,
    char **path,
    int path_len,
    int *path_capacity,
    int recurse,
    pal_listdir_callback_t callback)
{
    pal_dir_t *dir = NULL;
    struct pal_dirent_t *dent = NULL;
    int result = -1;

    dir = pal_opendir(*path);
    if (dir == NULL) {
        goto done;
    }

    while ((dent = pal_readdir(dir)) != NULL) {
        if (pal__fs_scandir_filter(dent)) {
            int path_len_new = pal_joinpath(is_absolute, *path, dent->d_name, path, 0, path_capacity);
            if (dent->d_type & PAL_DT_DIR) {
                if (callback(context, *path, 1) != 0) {
                    goto done;
                }
                if (recurse) {
                    if (pal_listdir_recurse(is_absolute, context, path, path_len_new, path_capacity, recurse, callback) != 0) {
                        goto done;
                    }
                }
            } else {
                if (callback(context, *path, 0) != 0) {
                    goto done;
                }
            }
            (*path)[path_len] = '\0';
        }
    }
    result = 0;
done:
    if (dir != NULL) {
        pal_closedir(dir);
    }
    return result;
}

int pal_listdir(void *context, const char *path, int recurse, pal_listdir_callback_t callback)
{
    int result = -1;
    int path_capacity = 128;
    char *search_path = NULL;
    int is_absolute = cwk_path_is_absolute(path);
    int path_len = pal_joinpath(is_absolute, path, ".", &search_path, 0, &path_capacity);
    if (search_path != NULL) {
        result = pal_listdir_recurse(is_absolute, context, &search_path, path_len, &path_capacity, recurse, callback);
        pal_free(search_path);
    }
    return result;
}

int pal_eof(pal_file_t file)
{
    int64_t cur = pal_lseek(file, 0, SEEK_CUR);
    int64_t end = pal_lseek(file, 0, SEEK_END);
    if (cur == end) {
        return 1;
    }
    pal_lseek(file, cur, SEEK_SET);
    return 0;
}

int pal_open_flags(const char *mode, const char *expected)
{
    int flags = 0;
    int has_r = 0;
    int has_plus = 0;
    int has_w = 0;
    int has_a = 0;
    if (mode[strspn(mode, "rwa+b")] != '\0') {
        return -1;
    }
    while (*mode != '\0') {
        if (*mode == 'r')
            has_r = 1;
        if (*mode == 'w')
            has_w = 1;
        if (*mode == 'a')
            has_a = 1;
        if (*mode == '+')
            has_plus = 1;
        mode++;
    }
    if (has_plus) {
        if (has_a) {
            flags = O_RDWR | O_CREAT | O_APPEND;
        } else if (has_w) {
            flags = O_RDWR | O_CREAT | O_TRUNC;
        } else if (has_r) {
            flags = O_RDWR;
        } else {
            return -1;
        }
    } else {
        if (has_a) {
            flags = O_WRONLY | O_CREAT | O_APPEND;
        } else if (has_w) {
            flags = O_WRONLY | O_CREAT | O_TRUNC;
        } else if (has_r) {
            flags = O_RDONLY;
        } else {
            return -1;
        }
    }
    return flags;
}

int pal_fgetc(pal_file_t fd)
{
    unsigned char ch;
    if (pal_read(fd, &ch, 1) < 1) {
        return EOF;
    }
    return ch;
}

int pal_fputc(int c, pal_file_t fd)
{
    unsigned char ch = c;
    int ret = pal_write(fd, &ch, 1);
    if (ret <= 0) {
        return EOF;
    }
    return c;
}
