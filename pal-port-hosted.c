
#include <fenv.h>
#include <stdlib.h>
#include <string.h>

#include "pal-port.h"

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__)
#include <malloc.h>
#elif defined(__FreeBSD__)
#include <malloc_np.h>
#endif

struct pal_session_t {
    pal_header_t header;
#if defined(_WIN32)
    wchar_t wname[2][PAL_PATH_MAX];
#endif
    const wchar_t **argv;
    const wchar_t **envp;
};

static pal_session_t global_pal;

#ifdef _WIN32
pal_process_info_t *pal_process_info_create_wchar(int argc, wchar_t **argv, wchar_t **envp)
{
    int idx;
    uint32_t envc;
    int total_str_count = argc + 1;
    const size_t ptr_size = sizeof(char **);
    char **argv_narrow;
    char **envp_narrow;
    char *str_ptr;
    pal_process_info_t *info;
    size_t total_str_length = 0;
    for (idx = 0; idx < argc; ++idx) {
        size_t n;
        dirent_wcstombs_s(
            &n, NULL, 0,
            argv[idx], SIZE_MAX);
        total_str_length += n;
    }
    for (idx = 0; envp[idx] != NULL; idx++) {
        size_t n;
        dirent_wcstombs_s(
            &n, NULL, 0,
            envp[idx], SIZE_MAX);
        total_str_length += n;
    }
    total_str_length = ((total_str_length + ptr_size - 1) / ptr_size) * ptr_size;
    envc = idx;
    total_str_count += envc + 1;
    info = (pal_process_info_t *)pal_mallocz(sizeof(pal_process_info_t) + total_str_count * ptr_size + total_str_length);
    argv_narrow = (char **)(info + 1);
    envp_narrow = argv_narrow + argc + 1;
    str_ptr = (char *)(envp_narrow + envc + 1);

    for (idx = 0; idx < argc; ++idx) {
        size_t n;
        dirent_wcstombs_s(
            &n, str_ptr, SIZE_MAX,
            argv[idx], SIZE_MAX);
        argv_narrow[idx] = str_ptr;
        str_ptr += n;
    }
    argv_narrow[idx] = NULL;
    for (idx = 0; idx < envc; idx++) {
        size_t n;
        dirent_wcstombs_s(
            &n, str_ptr, SIZE_MAX,
            envp[idx], SIZE_MAX);
        envp_narrow[idx] = str_ptr;
        str_ptr += n;
    }
    envp_narrow[idx] = NULL;
    pal_process_info_initialize(info, argc, argv_narrow, envc, envp_narrow);
    return info;
}
#endif

pal_process_info_t *pal_process_info_create(int argc, char **argv, char **envp)
{
    pal_process_info_t *info = (pal_process_info_t *)pal_mallocz(sizeof(pal_process_info_t));
    pal_process_info_initialize(info, argc, argv, -1, envp);
    return info;
}

void pal_initialize(int argc, void **argv, void **envp, bool is_wchar)
{
    pal_process_info_t *info;
    memset(&global_pal, 0, sizeof(global_pal));
#ifdef _WIN32
    if (is_wchar) {
        info = pal_process_info_create_wchar(argc, (wchar_t **)argv, (wchar_t **)envp);
    } else {
        info = pal_process_info_create(argc, (char **)argv, (char **)envp);
    }
#else
    info = pal_process_info_create(argc, (char **)argv, (char **)envp);
#endif
    pal_process_info_set(info);
}

pal_session_t *pal_global()
{
    return &global_pal;
}

void pal_finalize()
{
    pal_free(global_pal.header.info);
}

pal_session_t *pal_opensession()
{
    pal_session_t *pal = (pal_session_t *)pal_mallocz(sizeof(pal_session_t));
    memset(pal, 0, sizeof(pal_session_t));
    pal->header.cwd = pal_getcwd();
    pal->header.cwd_length = strlen(pal->header.cwd);
    pal->header.cwd_capacity = pal->header.cwd_length + 1;
    return pal;
}

void pal_closesession(pal_session_t *pal)
{
    pal_free(pal->header.cwd);
    pal_free(pal);
}

void pal_abort()
{
    abort();
}

void pal_exit(int exitcode)
{
    _exit(exitcode);
}

int pal_fesetround(pal_fround_t pal_round)
{
    int round;
    switch (pal_round) {
        PAL_TO_NATIVE_CASE(round, FE_TONEAREST);
        PAL_TO_NATIVE_CASE(round, FE_DOWNWARD);
        PAL_TO_NATIVE_CASE(round, FE_UPWARD);
        PAL_TO_NATIVE_CASE(round, FE_TOWARDZERO);
    default:
        errno = EINVAL;
        return -1;
    }
    return fesetround(round);
}

void *pal_malloc(size_t sz)
{
    return malloc(sz);
}

void *pal_mallocz(size_t sz)
{
    void *ptr = pal_malloc(sz);
    if (ptr != NULL) {
        memset(ptr, 0, sz);
    }
    return ptr;
}

void *pal_realloc(void *__ptr, size_t __size)
{
    return realloc(__ptr, __size);
}

/* default memory allocation functions with memory limitation */
size_t pal_malloc_usable_size(const void *ptr)
{
#if defined(__APPLE__)
    return malloc_size(ptr);
#elif defined(_WIN32)
    return _msize((void *)ptr);
#elif defined(EMSCRIPTEN)
    return 0;
#elif defined(__linux__)
    return malloc_usable_size((void *)ptr);
#else
    /* change this to `return 0;` if compilation fails */
    return malloc_usable_size(ptr);
#endif
}

void pal_free(void *ptr)
{
    if (ptr != NULL) {
        free(ptr);
    }
}

pal_file_t pal_popen(const char *process_command, int flags)
{
    const char *mode = "r";
    FILE *f;
    pal_file_t fd = -1;
    if (flags != 0) {
        mode = "w";
    }
#if defined(_WIN32)
#define popen _popen
#define pclose _pclose
#endif
    f = popen(process_command, mode);
    if (f != NULL) {
        fd = dup(fileno(f));
        pclose(f);
    }
    return fd;
}

pal_file_t pal_file_get(pal_file_type_t type)
{
    return type;
}

pal_file_t pal_file_open_tmp()
{
    FILE *tmp = tmpfile();
    pal_file_t f = -1;
    if (tmp) {
        f = dup(fileno(tmp));
        fclose(tmp);
    }
    return f;
}

int pal_read(pal_file_t file, void *buffer, uint32_t sz)
{
    return read(file, buffer, sz);
}

int pal_writes(pal_file_t file, const char *str)
{
    return pal_write(file, str, (uint32_t)strlen(str));
}

int pal_write(pal_file_t file, const void *buffer, uint32_t sz)
{
    if (file < 3) {
        file = file;
    }
    return write(file, buffer, sz);
}

int pal_close(pal_file_t file)
{
    if (file >= 0) {
        /* Close a file */
        return close(file);
    }
    errno = EINVAL;
    return -1;
}

int pal_pclose(pal_file_t file)
{
    return pal_close(file);
}

int pal_dup(pal_file_t fd)
{
    return dup(fd);
}

int pal_dup2(pal_file_t fd1, pal_file_t fd2)
{
    return dup2(fd1, fd2);
}

int pal_tty_isatty(pal_file_t fd)
{
    return isatty(fd);
}
