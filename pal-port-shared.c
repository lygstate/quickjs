#include <fcntl.h>
#include <assert.h>

#include "cwalk.h"
#include "pal-port.h"

#define PAL_EV_IS_INITED_AND_FD_IN_RANGE(fd) \
    (((unsigned)fd) < (unsigned)pal_ev.max_fd)
#define PAL_EV_TOO_MANY_LOOPS (pal_ev.num_loops != 0) /* use after ++ */
#define PAL_EV_FD_BELONGS_TO_LOOP(loop, fd) \
    ((loop)->loop_id == pal_ev.fds[fd].loop_id)

#define PAL_EV_TIMEOUT_VEC_OF(loop, idx) \
    ((loop)->timeout.vec + (idx)*pal_ev.timeout_vec_size)
#define PAL_EV_TIMEOUT_VEC_OF_VEC_OF(loop, idx) \
    ((loop)->timeout.vec_of_vec + (idx)*pal_ev.timeout_vec_of_vec_size)
#define PAL_EV_RND_UP(v, d) (((v) + (d)-1) / (d) * (d))

pal_ev_globals_t pal_ev;

void *pal_mallocz(size_t sz)
{
    void *ptr = pal_malloc(sz);
    if (ptr != NULL) {
        memset(ptr, 0, sz);
    }
    return ptr;
}

char *pal_strdup(const char *str)
{
    size_t str_capacity = strlen(str) + 1;
    void *ptr = pal_malloc(str_capacity);
    if (ptr != NULL) {
        memcpy(ptr, str, str_capacity);
    }
    return ptr;
}

pal_process_info_t *pal_process_info_create(int argc, char **argv, char **envp)
{
    pal_process_info_t *info = (pal_process_info_t *)pal_mallocz(sizeof(pal_process_info_t));
    pal_process_info_initialize(info, argc, argv, -1, envp);
    return info;
}

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
    int ret = -1;
    uint32_t dir_len = 0;
    char *abs_dirpath = NULL;
    if (dirpath) {
        abs_dirpath = pal_session_realpath(pal, dirpath);
    }
    if (abs_dirpath) {
        pal_stat_t st;
        if (pal_stat(abs_dirpath, &st, 0) >= 0) {
            if (S_ISDIR(st.st_mode)) {
                dir_len = strlen(abs_dirpath);
            }
        }
    }
    if (dir_len > 0 && abs_dirpath) {
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
            memcpy(pal_header->cwd, abs_dirpath, dir_len + 1);
            ret = 0;
        }
    } else {
        errno = ENOTDIR;
    }
    pal_free(abs_dirpath);
    return ret;
}

int pal_mkdir_recursive(const char *dirpath, pal_mode_t mode)
{
    int ret = -1;
    char *abs_dirpath = pal_realpath(dirpath);
    struct cwk_segment segment;
    if (abs_dirpath) {
        bool has_segment = cwk_path_get_first_segment(abs_dirpath, &segment);
        while (has_segment) {
            char end_saved = *(segment.end);
            *(char *)(segment.end) = '\0';
            ret = pal_mkdir(abs_dirpath, mode);
            *(char *)(segment.end) = end_saved;
            has_segment = cwk_path_get_next_segment(&segment);
        }
        pal_free(abs_dirpath);
    }
    return ret;
}

int pal_joinpath(int is_absolute, const char *base_path, const char *from_path, char **resolved_path, int offset, int *capacity)
{
    int base_path_with_offset_len;
    const char *base_path_with_offset = base_path + offset;
    if (*capacity < 0 || base_path == NULL || from_path == NULL || resolved_path == NULL || capacity == NULL) {
        errno = EINVAL;
        return -1;
    }
    base_path_with_offset_len = (int)strlen(base_path_with_offset);
    for (;;) {
        int len;
        int buffer_size = 0;
        int new_capacity;
        char *resolved_path_with_offset = *resolved_path == NULL ? NULL : (*resolved_path + offset);
        if (resolved_path_with_offset) {
            buffer_size = *capacity - offset;
            if (buffer_size < 0) {
                buffer_size = 0;
            }
        }
        if (is_absolute) {
            len = (int)cwk_path_get_absolute(base_path_with_offset, from_path, resolved_path_with_offset, buffer_size);
        } else {
            len = (int)cwk_path_join(base_path_with_offset, from_path, resolved_path_with_offset, buffer_size);
        }
        new_capacity = offset + len + 1;
        if (new_capacity <= *capacity) {
            if (resolved_path_with_offset != NULL) {
                return len;
            }
        } else {
            *capacity = new_capacity;
        }
        if (base_path_with_offset == resolved_path_with_offset) {
            resolved_path_with_offset[base_path_with_offset_len] = '\0';
            *resolved_path = pal_realloc(*resolved_path, *capacity);
            base_path_with_offset = *resolved_path + offset;
        } else {
            *resolved_path = pal_realloc(*resolved_path, *capacity);
        }
        if (*resolved_path == NULL) {
            errno = ENOMEM;
            return -1;
        }
    }
}

char *pal_realpath(const char *path)
{
    char *abspath = NULL;
    int abspath_capacity = 0;
    int ret = -1;
    if (path) {
        char *cwd = pal_getcwd();
        if (cwd) {
            ret = pal_joinpath(1, cwd, path, &abspath, 0, &abspath_capacity);
            pal_free(cwd);
        }
    }
    if (ret < 0 || abspath == NULL) {
        errno = EINVAL;
    }
    return abspath;
}

char *pal_dirname(const char *path)
{
    char *dirname = NULL;
    size_t dirname_length = 0;
    if (path != NULL) {
        cwk_path_get_dirname(path, &dirname_length);
    }
    if (dirname_length > 0) {
        dirname = pal_malloc(dirname_length + 1);
        if (dirname != NULL) {
            memcpy(dirname, path, dirname_length);
            dirname[dirname_length] = 0;
        }
    } else {
        errno = ENOTDIR;
    }
    return dirname;
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

void *pal_ev_memalign(size_t sz, void **orig_addr, int clear)
{
    sz = sz + PAL_EV_PAGE_SIZE + PAL_EV_CACHE_LINE_SIZE;
    if ((*orig_addr = malloc(sz)) == NULL) {
        return NULL;
    }
    if (clear != 0) {
        memset(*orig_addr, 0, sz);
    }
    return (void *)PAL_EV_RND_UP((unsigned long)*orig_addr + (rand() % PAL_EV_PAGE_SIZE),
                                 PAL_EV_CACHE_LINE_SIZE);
}

int pal_ev_init(int max_fd)
{
    assert(!PAL_EV_IS_INITED(pal_ev));
    assert(max_fd > 0);
    if ((pal_ev.fds = (pal_ev_fd_t *)pal_ev_memalign(sizeof(pal_ev_fd_t) * max_fd,
                                                     &pal_ev._fds_free_addr, 1)) == NULL) {
        return -1;
    }
    pal_ev.max_fd = max_fd;
    pal_ev.num_loops = 0;
    pal_ev.timeout_vec_size = PAL_EV_RND_UP(pal_ev.max_fd, PAL_EV_SIMD_BITS) / PAL_EV_SHORT_BITS;
    pal_ev.timeout_vec_of_vec_size = PAL_EV_RND_UP(pal_ev.timeout_vec_size, PAL_EV_SIMD_BITS) / PAL_EV_SHORT_BITS;
    return 0;
}

int pal_ev_deinit(void)
{
    assert(PAL_EV_IS_INITED(pal_ev));
    free(pal_ev._fds_free_addr);
    pal_ev.fds = NULL;
    pal_ev._fds_free_addr = NULL;
    pal_ev.max_fd = 0;
    pal_ev.num_loops = 0;
    return 0;
}

void pal_ev_set_timeout(pal_ev_loop_t *loop, int fd, int timeout_in_usecs)
{
    pal_ev_fd_t *target;
    short *vec, *vec_of_vec;
    size_t vi = fd / PAL_EV_SHORT_BITS;
    int64_t delta;
    assert(PAL_EV_IS_INITED_AND_FD_IN_RANGE(fd));
    assert(PAL_EV_FD_BELONGS_TO_LOOP(loop, fd));
    target = pal_ev.fds + fd;
    /* clear timeout */
    if (target->timeout_idx != PAL_EV_TIMEOUT_IDX_UNUSED) {
        vec = PAL_EV_TIMEOUT_VEC_OF(loop, target->timeout_idx);
        if ((vec[vi] &= ~((unsigned short)SHRT_MIN >> (fd % PAL_EV_SHORT_BITS))) == 0) {
            vec_of_vec = PAL_EV_TIMEOUT_VEC_OF_VEC_OF(loop, target->timeout_idx);
            vec_of_vec[vi / PAL_EV_SHORT_BITS] &= ~((unsigned short)SHRT_MIN >> (vi % PAL_EV_SHORT_BITS));
        }
        target->timeout_idx = PAL_EV_TIMEOUT_IDX_UNUSED;
    }
    if (secs != 0) {
        delta = (loop->now - loop->timeout.base_time) / loop->timeout.resolution;
        if (delta >= PAL_EV_TIMEOUT_VEC_SIZE) {
            delta = PAL_EV_TIMEOUT_VEC_SIZE - 1;
        }
        target->timeout_idx =
            (loop->timeout.base_idx + delta) % PAL_EV_TIMEOUT_VEC_SIZE;
        vec = PAL_EV_TIMEOUT_VEC_OF(loop, target->timeout_idx);
        vec[vi] |= (unsigned short)SHRT_MIN >> (fd % PAL_EV_SHORT_BITS);
        vec_of_vec = PAL_EV_TIMEOUT_VEC_OF_VEC_OF(loop, target->timeout_idx);
        vec_of_vec[vi / PAL_EV_SHORT_BITS] |= (unsigned short)SHRT_MIN >> (vi % PAL_EV_SHORT_BITS);
    }
}

int pal_ev_add(pal_ev_loop_t *loop, int fd, int events, int timeout_in_secs,
               pal_ev_handler_t *callback, void *cb_arg)
{
    pal_ev_fd_t *target;
    assert(PAL_EV_IS_INITED_AND_FD_IN_RANGE(fd));
    target = pal_ev.fds + fd;
    assert(target->loop_id == 0);
    target->callback = callback;
    target->cb_arg = cb_arg;
    target->loop_id = loop->loop_id;
    target->events = 0;
    target->timeout_idx = PAL_EV_TIMEOUT_IDX_UNUSED;
    if (pal_ev_update_events_internal(loop, fd, events | PAL_EV_ADD) != 0) {
        target->loop_id = 0;
        return -1;
    }
    pal_ev_set_timeout(loop, fd, timeout_in_usecs);
    return 0;
}

int pal_ev_del(pal_ev_loop_t *loop, int fd)
{
    pal_ev_fd_t *target;
    assert(PAL_EV_IS_INITED_AND_FD_IN_RANGE(fd));
    target = pal_ev.fds + fd;
    if (pal_ev_update_events_internal(loop, fd, PAL_EV_DEL) != 0) {
        return -1;
    }
    pal_ev_set_timeout(loop, fd, 0);
    target->loop_id = 0;
    return 0;
}

int pal_ev_is_active(pal_ev_loop_t *loop, int fd)
{
    assert(PAL_EV_IS_INITED_AND_FD_IN_RANGE(fd));
    return loop != NULL
               ? pal_ev.fds[fd].loop_id == loop->loop_id
               : pal_ev.fds[fd].loop_id != 0;
}

int pal_ev_get_events(pal_ev_loop_t *loop pal_maybe_unused, int fd)
{
    assert(PAL_EV_IS_INITED_AND_FD_IN_RANGE(fd));
    return pal_ev.fds[fd].events & PAL_EV_READWRITE;
}

int pal_ev_set_events(pal_ev_loop_t *loop, int fd, int events)
{
    assert(PAL_EV_IS_INITED_AND_FD_IN_RANGE(fd));
    if (pal_ev.fds[fd].events != events && pal_ev_update_events_internal(loop, fd, events) != 0) {
        return -1;
    }
    return 0;
}

pal_ev_handler_t *pal_ev_get_callback(pal_ev_loop_t *loop pal_maybe_unused,
                                      int fd, void **cb_arg)
{
    assert(PAL_EV_IS_INITED_AND_FD_IN_RANGE(fd));
    if (cb_arg != NULL) {
        *cb_arg = pal_ev.fds[fd].cb_arg;
    }
    return pal_ev.fds[fd].callback;
}

void pal_ev_set_callback(pal_ev_loop_t *loop pal_maybe_unused, int fd,
                         pal_ev_handler_t *callback, void **cb_arg)
{
    assert(PAL_EV_IS_INITED_AND_FD_IN_RANGE(fd));
    if (cb_arg != NULL) {
        pal_ev.fds[fd].cb_arg = *cb_arg;
    }
    pal_ev.fds[fd].callback = callback;
}

int pal_ev_next_fd(pal_ev_loop_t *loop, int curfd)
{
    if (curfd != -1) {
        assert(PAL_EV_IS_INITED_AND_FD_IN_RANGE(curfd));
    }
    while (++curfd < pal_ev.max_fd) {
        if (loop->loop_id == pal_ev.fds[curfd].loop_id) {
            return curfd;
        }
    }
    return -1;
}

int pal_ev_init_loop_internal(pal_ev_loop_t *loop, int max_timeout)
{
    loop->loop_id = ++pal_ev.num_loops;
    assert(PAL_EV_TOO_MANY_LOOPS);
    if ((loop->timeout.vec_of_vec = (short *)pal_ev_memalign((pal_ev.timeout_vec_of_vec_size + pal_ev.timeout_vec_size) * sizeof(short) * PAL_EV_TIMEOUT_VEC_SIZE,
                                                             &loop->timeout._free_addr, 1)) == NULL) {
        --pal_ev.num_loops;
        return -1;
    }
    loop->timeout.vec = loop->timeout.vec_of_vec + pal_ev.timeout_vec_of_vec_size * PAL_EV_TIMEOUT_VEC_SIZE;
    loop->timeout.base_idx = 0;
    pal_clock_gettime(PAL_CLOCK_MONOTONIC, &loop->timeout.base_time);
    loop->timeout.resolution = PAL_EV_RND_UP(max_timeout, PAL_EV_TIMEOUT_VEC_SIZE) / PAL_EV_TIMEOUT_VEC_SIZE;
    pal_clock_gettime(PAL_CLOCK_MONOTONIC, &loop->now);
    return 0;
}

void pal_ev_deinit_loop_internal(pal_ev_loop_t *loop)
{
    free(loop->timeout._free_addr);
}

void pal_ev_handle_timeout_internal(pal_ev_loop_t *loop)
{
    size_t i, j, k;
    for (;
         loop->timeout.base_time <= loop->now - loop->timeout.resolution;
         loop->timeout.base_idx = (loop->timeout.base_idx + 1) % PAL_EV_TIMEOUT_VEC_SIZE,
         loop->timeout.base_time += loop->timeout.resolution) {
        /* TODO use SIMD instructions */
        short *vec = PAL_EV_TIMEOUT_VEC_OF(loop, loop->timeout.base_idx);
        short *vec_of_vec = PAL_EV_TIMEOUT_VEC_OF_VEC_OF(loop, loop->timeout.base_idx);
        for (i = 0; i < pal_ev.timeout_vec_of_vec_size; ++i) {
            short vv = vec_of_vec[i];
            if (vv != 0) {
                for (j = i * PAL_EV_SHORT_BITS; vv != 0; j++, vv <<= 1) {
                    if (vv < 0) {
                        short v = vec[j];
                        assert(v != 0);
                        for (k = j * PAL_EV_SHORT_BITS; v != 0; k++, v <<= 1) {
                            if (v < 0) {
                                pal_ev_fd_t *fd = pal_ev.fds + k;
                                assert(fd->loop_id == loop->loop_id);
                                fd->timeout_idx = PAL_EV_TIMEOUT_IDX_UNUSED;
                                (*fd->callback)(loop, k, PAL_EV_TIMEOUT, fd->cb_arg);
                            }
                        }
                        vec[j] = 0;
                    }
                }
                vec_of_vec[i] = 0;
            }
        }
    }
}

int pal_ev_loop_once(pal_ev_loop_t *loop, int max_wait)
{
    loop->now = time(NULL);
    if (max_wait > loop->timeout.resolution) {
        max_wait = loop->timeout.resolution;
    }
    if (pal_ev_poll_once_internal(loop, max_wait) != 0) {
        return -1;
    }
    if (max_wait != 0) {
        loop->now = time(NULL);
    }
    pal_ev_handle_timeout_internal(loop);
    return 0;
}
