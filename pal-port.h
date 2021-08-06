#ifndef _PAL_PORT_H_
#define _PAL_PORT_H_

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#if defined(_MSC_VER)
#include "win/msvc_stdatomic.h"
#else
#include <stdatomic.h>
#include <stdbool.h>
#endif

#if defined(_MSC_VER)
#define pal_no_return __declspec(noreturn)
#define pal_force_inline __forceinline
#define pal_maybe_unused
#else
#define pal_no_return __attribute__((noreturn))
#define pal_force_inline inline __attribute__((always_inline))
#define pal_maybe_unused __attribute__((unused))
#endif

/* The maximal path length, + 16 is for 'file://' prefix */
#if defined(_WIN32)
#define PAL_PATH_MAX 65536 /* Big enough */
#elif defined(PATH_MAX)
#define PAL_PATH_MAX (PATH_MAX + 16)
#else
#define PAL_PATH_MAX (256 + 16)
#endif

/* 1024 comes from OSX, choose the maximal value across all platform */
#define PAL_FILENAME_MAX 1024

#define PAL_TO_NATIVE_CASE(value, enum_name) \
    case PAL_##enum_name: {                  \
        value = enum_name;                   \
        break;                               \
    }

#define PAL_FROM_NATIVE_CASE(value, enum_name) \
    case enum_name: {                          \
        value = PAL_##enum_name;               \
        break;                                 \
    }

#define pal_in_range(c, lo, up)  ((uint8_t)(c) >= (lo) && (uint8_t)(c) <= (up))
#define pal_isdigit(c)           pal_in_range((c), '0', '9')
#define pal_isxdigit(c)          (pal_isdigit(c) || pal_in_range((c), 'a', 'f') || pal_in_range((c), 'A', 'F'))
#define pal_islower(c)           pal_in_range((c), 'a', 'z')
#define pal_isspace(c)           ((c) == ' ' || (c) == '\f' || (c) == '\n' || (c) == '\r' || (c) == '\t' || (c) == '\v')
#define pal_isupper(c)           pal_in_range((c), 'A', 'Z')
#define pal_tolower(c)           (pal_isupper(c) ? (c) - 'A' + 'a' : c)
#define pal_toupper(c)           (pal_islower(c) ? (c) - 'a' + 'A' : c)
#define pal_isalpha(c)           (pal_islower(c) || pal_isupper(c))

/**
 * @section These predefined macros is used for pal_stat_t.st_mode
 * as a supplement for sys/stat.h
 */

/* File type and permission flags for stat(), general mask */
#if !defined(S_IFMT)
#define S_IFMT _S_IFMT
#endif

/* Directory bit */
#if !defined(S_IFDIR)
#define S_IFDIR _S_IFDIR
#endif

/* Character device bit */
#if !defined(S_IFCHR)
#define S_IFCHR _S_IFCHR
#endif

/* Regular file bit */
#if !defined(S_IFREG)
#define S_IFREG _S_IFREG
#endif

/* Read permission */
#if !defined(S_IREAD)
#define S_IREAD _S_IREAD
#endif

/* Write permission */
#if !defined(S_IWRITE)
#define S_IWRITE _S_IWRITE
#endif

/* Execute permission */
#if !defined(S_IEXEC)
#define S_IEXEC _S_IEXEC
#endif

/* Pipe */
#if !defined(S_IFIFO)
#define S_IFIFO _S_IFIFO
#endif

/* Block device */
#if !defined(S_IFBLK)
#define S_IFBLK 0x6000 /* 0060000 */
#endif

/* Link */
#if !defined(S_IFLNK)
#define S_IFLNK 0xA000 /* 0120000 */
#endif

/* Socket */
#if !defined(S_IFSOCK)
#define S_IFSOCK 0xC000 /* 0140000 */
#endif

/* Group ID */
#if !defined(S_ISGID)
#define S_ISGID 0x400
#endif

/* User ID */
#if !defined(S_ISUID)
#define S_ISUID 0x800
#endif

/* Read user permission */
#if !defined(S_IRUSR)
#define S_IRUSR S_IREAD
#endif

/* Write user permission */
#if !defined(S_IWUSR)
#define S_IWUSR S_IWRITE
#endif

/* Execute user permission */
#if !defined(S_IXUSR)
#define S_IXUSR S_IEXEC
#endif

/* Read group permission */
#if !defined(S_IRGRP)
#define S_IRGRP S_IREAD
#endif

/* Write group permission */
#if !defined(S_IWGRP)
#define S_IWGRP S_IWRITE
#endif

/* Execute group permission */
#if !defined(S_IXGRP)
#define S_IXGRP S_IEXEC
#endif

/* Read others permission */
#if !defined(S_IROTH)
#define S_IROTH S_IREAD
#endif

/* Write others permission */
#if !defined(S_IWOTH)
#define S_IWOTH S_IWRITE
#endif

/* Execute others permission */
#if !defined(S_IXOTH)
#define S_IXOTH S_IEXEC
#endif

#if !defined(S_IRWXU)
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#endif

#if !defined(S_IRWXG)
#define S_IRWXG (S_IRGRP | S_IWGRP | S_IXGRP)
#endif

#if !defined(S_IRWXO)
#define S_IRWXO (S_IROTH | S_IWOTH | S_IXOTH)
#endif

/*
 * File type macros.  Note that block devices, sockets and links cannot be
 * distinguished on Windows and the macros S_ISBLK, S_ISSOCK and S_ISLNK are
 * only defined for compatibility.  These macros should always return false
 * on Windows.
 */
#if !defined(S_ISFIFO)
#define S_ISFIFO(mode) (((mode)&S_IFMT) == S_IFIFO)
#endif
#if !defined(S_ISDIR)
#define S_ISDIR(mode) (((mode)&S_IFMT) == S_IFDIR)
#endif
#if !defined(S_ISREG)
#define S_ISREG(mode) (((mode)&S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISLNK)
#define S_ISLNK(mode) (((mode)&S_IFMT) == S_IFLNK)
#endif
#if !defined(S_ISSOCK)
#define S_ISSOCK(mode) (((mode)&S_IFMT) == S_IFSOCK)
#endif
#if !defined(S_ISCHR)
#define S_ISCHR(mode) (((mode)&S_IFMT) == S_IFCHR)
#endif
#if !defined(S_ISBLK)
#define S_ISBLK(mode) (((mode)&S_IFMT) == S_IFBLK)
#endif

/* For waitpid */
#ifndef WNOHANG
#define WNOHANG 1
#endif
/**
 * @section These predefined macros are used for dynamic library
 * loading functions
 */
/* For dynamic library loading */
#define PAL_RTLD_LAZY 0x00001     /* Lazy function call binding.  */
#define PAL_RTLD_NOW 0x00002      /* Immediate function call binding.  */
#define PAL_RTLD_BINDING_MASK 0x3 /* Mask of binding time value.  */
#define PAL_RTLD_NOLOAD 0x00004   /* Do not load the object.  */
#define PAL_RTLD_DEEPBIND 0x00008 /* Use deep binding.  */

/* If the following bit is set in the MODE argument to `dlopen',
   the symbols of the loaded object and its dependencies are made
   visible as if the object were linked directly into the program.  */
#define PAL_RTLD_GLOBAL 0x00100

/* Unix98 demands the following flag which is the inverse to RTLD_GLOBAL.
   The implementation does this by default and so we can define the
   value to zero.  */
#define PAL_RTLD_LOCAL 0

/* Do not delete object when closed.  */
#define PAL_RTLD_NODELETE 0x01000

/**
 * @section These predefined macros used for pal event loop library
 */
#define PAL_EV_IS_INITED(pal_ev) (pal_ev.max_fd != 0)
#define PAL_EV_PAGE_SIZE 4096
#define PAL_EV_CACHE_LINE_SIZE 32 /* in bytes, ok if greater than the actual */
#define PAL_EV_SIMD_BITS 128
#define PAL_EV_TIMEOUT_VEC_SIZE 128
#define PAL_EV_SHORT_BITS (sizeof(short) * 8)

#define PAL_EV_READ 1
#define PAL_EV_WRITE 2
#define PAL_EV_TIMEOUT 4
#define PAL_EV_ADD 0x40000000
#define PAL_EV_DEL 0x20000000
#define PAL_EV_READWRITE (PAL_EV_READ | PAL_EV_WRITE)

#define PAL_EV_TIMEOUT_IDX_UNUSED (UCHAR_MAX)

enum pal_clockid_t {
    PAL_CLOCK_REALTIME = 0,
    PAL_CLOCK_MONOTONIC = 1,
    PAL_CLOCK_PROCESS_CPUTIME_ID = 2,
    PAL_CLOCK_THREAD_CPUTIME_ID = 3,
    PAL_CLOCK_REALTIME_COARSE = 4,
};
typedef enum pal_clockid_t pal_clockid_t;

enum pal_fround_t {
    PAL_FE_TONEAREST = 0x0000,
    PAL_FE_DOWNWARD = 0x0400,
    PAL_FE_UPWARD = 0x0800,
    PAL_FE_TOWARDZERO = 0x0c00,
};
typedef enum pal_fround_t pal_fround_t;

/* File types for `d_type'.  */
enum pal_d_type_t {
    PAL_DT_UNKNOWN = 0,
    PAL_DT_FIFO = 1,
    PAL_DT_CHR = 2,
    PAL_DT_DIR = 4,
    PAL_DT_BLK = 6,
    PAL_DT_REG = 8,
    PAL_DT_LNK = 10,
    PAL_DT_SOCK = 12,
    PAL_DT_WHT = 14
};
typedef enum pal_d_type_t pal_d_type_t;

/* Multi-byte character version */
struct pal_dirent_t {
    void *native;

    /* Always zero */
    int64_t d_ino;

    /* File position within stream */
    int64_t d_off;

    /* Structure size */
    unsigned short d_reclen;

    /* Length of name without \0 */
    size_t d_namlen;

    /* File type */
    unsigned char d_type;

    /* File name */
    char d_name[PAL_FILENAME_MAX];
};
typedef struct pal_dirent_t pal_dirent_t;

struct pal_dir_t {
    pal_dirent_t ent;
    uint32_t native_state; /* An state value to add simple native handling */
    struct pal_dir_native_t *native;
};
typedef struct pal_dir_t pal_dir_t;
typedef struct pal_dir_native_t pal_dir_native_t;

typedef uint64_t pal_dev_t;
typedef uint64_t pal_ino_t;
typedef unsigned pal_nlink_t;
typedef unsigned pal_mode_t;
typedef unsigned pal_uid_t;
typedef unsigned pal_gid_t;
typedef int pal_pid_t;
typedef int64_t pal_off_t;
typedef long pal_blksize_t;
typedef int64_t pal_blkcnt_t;

struct pal_stat_t {
    pal_dev_t st_dev;     /* Device.  */
    pal_ino_t st_ino;     /* File serial number.  */
    pal_nlink_t st_nlink; /* Link count.  */
    pal_mode_t st_mode;   /* File mode.  */
    pal_uid_t st_uid;     /* User ID of the file's owner.	*/
    pal_gid_t st_gid;     /* Group ID of the file's group.*/

    pal_dev_t st_rdev;        /* Device number, if device.  */
    pal_off_t st_size;        /* Size of file, in bytes.  */
    pal_blksize_t st_blksize; /* Optimal block size for I/O.  */
    pal_blkcnt_t st_blocks;   /* Nr. 512-byte blocks allocated.  */
    /* Nanosecond resolution timestamps are stored in a format
       equivalent to 'struct timespec'.  This is the type used
       whenever possible but the Unix namespace rules do not allow the
       identifier 'timespec' to appear in the <sys/stat.h> header.
       Therefore we have to handle the use of this header in strictly
       standard-compliant sources special.  */
    struct timespec st_atim; /* Time of last access.  */
    struct timespec st_mtim; /* Time of last modification.  */
    struct timespec st_ctim; /* Time of last status change.  */
};
typedef struct pal_stat_t pal_stat_t;

enum pal_file_type_t {
    PAL_FILE_STDIN = 0,
    PAL_FILE_STDOUT = 1,
    PAL_FILE_STDERR = 2,
};
typedef enum pal_file_type_t pal_file_type_t;
typedef int pal_file_t;

struct pal_process_info_t {
    uint32_t argc;
    char **argv;
    uint32_t envc;
    char **envp;
    char *executable_path; /* The path to the current executable */
    char *executable_dir; /* The path to the current dirname */
};
typedef struct pal_process_info_t pal_process_info_t;

struct pal_header_t {
    pal_process_info_t *info;
    char *cwd;
    uint32_t cwd_capacity;
    uint32_t cwd_length;
};
typedef struct pal_header_t pal_header_t;

typedef struct pal_session_t pal_session_t;

/**
 * @section Thread related types
 */

//! @brief The thread method.
typedef void *(*pal_thread_method)(void *);

typedef void *pal_thread;
typedef void *pal_mutex;
typedef void *pal_condition;

/**
 * @section The pal event loop library is a fork of
 *  https://github.com/kazuho/picoev,
 *  renamed the prefix from pal_ev to pal_ev, the new api are
 *  not compatible with picoev.
 */
typedef unsigned short pal_ev_loop_id_t;

typedef struct pal_ev_loop_t pal_ev_loop_t;

typedef void pal_ev_handler_t(pal_ev_loop_t *loop, int fd, int revents,
                              void *cb_arg);

typedef struct pal_ev_fd_t {
    /* use accessors! */
    /* TODO adjust the size to match that of a cache line */
    pal_ev_handler_t *callback;
    void *cb_arg;
    pal_ev_loop_id_t loop_id;
    char events;
    unsigned char timeout_idx; /* PAL_EV_TIMEOUT_IDX_UNUSED if not used */
    int _backend;              /* can be used by backends (never modified by core) */
} pal_ev_fd_t;

struct pal_ev_loop_t {
    /* read only */
    pal_ev_loop_id_t loop_id;
    struct {
        short *vec;
        short *vec_of_vec;
        size_t base_idx;
        struct timespec base_time;
        int resolution;
        void *_free_addr;
    } timeout;
    struct timespec now;
};

typedef struct pal_ev_globals_t {
    /* read only */
    pal_ev_fd_t *fds;
    void *_fds_free_addr;
    int max_fd;
    int num_loops;
    size_t timeout_vec_size;        /* # of elements in pal_ev_loop_t.timeout.vec[0] */
    size_t timeout_vec_of_vec_size; /* ... in timeout.vec_of_vec[0] */
} pal_ev_globals_t;

#ifndef pal_get_stack_pointer_defined
/* Note: OS and CPU dependent */
static pal_force_inline uintptr_t pal_get_stack_pointer(void)
{
#if defined(EMSCRIPTEN)
    return 0;
#elif defined(_MSC_VER)
    return (uintptr_t)_AddressOfReturnAddress();
#else
    return (uintptr_t)__builtin_frame_address(0);
#endif
}
#endif

void pal_no_return pal_abort();
void pal_no_return pal_exit(int exitcode);

/* Memory relate functions */
void *pal_malloc(size_t __size);
void *pal_realloc(void *__ptr, size_t __size);
void pal_free(void *__ptr);
size_t pal_malloc_usable_size(const void *ptr);

int pal_clock_gettime(pal_clockid_t clock_id, struct timespec *tp);
/**
 * @brief get timezone offset
 * @param time UTC epoch time in seconds
 * @return timezone offset in minute
 *  For example
 *   GTM+8 will return 480 minutes= 8 * 60 seconds
 *   UTC  will return 0
 *   GTM-8 will return -480 minutes= -8 * 60 seconds
 */
int pal_gettimezoneoffset(int64_t time);

int pal_fesetround(pal_fround_t pal_round);

int pal_msleep(int64_t ms);
int pal_usleep(int64_t us);

//! @brief Detaches a thread.
int pal_thread_create(pal_thread *thread, pal_thread_method method, void *data, int detached);

//! @brief Joins a thread.
int pal_thread_join(pal_thread *thread);

/* Mutex relaed functions */
int pal_mutex_init(pal_mutex *mutex);

//! @brief Locks a mutex.
int pal_mutex_lock(pal_mutex *mutex);

//! @brief Tries to locks a mutex.
int pal_mutex_trylock(pal_mutex *mutex);

//! @brief Unlocks a mutex.
int pal_mutex_unlock(pal_mutex *mutex);

//! @brief Destroy a mutex.
int pal_mutex_destroy(pal_mutex *mutex);

//! @brief Initializes a condition.
int pal_condition_init(pal_condition *cond);

//! @brief Restarts one of the threads that are waiting on the condition.
int pal_condition_signal(pal_condition *cond);

//! @brief Shall unblock all threads currently blocked on the specified condition variable cond.
int pal_condition_broadcast(pal_condition *cond);

//! @brief Unlocks the mutex and waits for the condition to be signalled.
int pal_condition_wait(pal_condition *cond, pal_mutex *mutex);

//! @brief Unlocks the mutex and waits for the condition to be signalled or timeout elapsed.
int pal_condition_timedwait(pal_condition *cond, pal_mutex *mutex, struct timespec *timeout);

//! @brief Destroy a condition.
int pal_condition_destroy(pal_condition *cond);

pal_process_info_t *pal_process_info_get();
void pal_process_info_set(pal_process_info_t *new_info);
void pal_process_info_initialize(pal_process_info_t *info, uint32_t argc, char **argv, uint32_t envc, char **envp);
char *pal_process_executable_path();
char *pal_getenv(const char*name);
int pal_setenv(const char *name, const char *value, int overwrite);
int pal_unsetenv(const char *name);

#ifdef _WIN32
int dirent_mbstowcs_s(
    size_t *pReturnValue, wchar_t *wcstr, size_t sizeInWords,
    const char *mbstr, size_t count);
int dirent_wcstombs_s(
    size_t *pReturnValue, char *mbstr, size_t sizeInBytes,
    const wchar_t *wcstr, size_t count);
pal_process_info_t *pal_process_info_create_wide(int argc, wchar_t **argv, wchar_t **envp);
#endif
pal_process_info_t *pal_process_info_create(int argc, char **argv, char **envp);

void pal_initialize(int argc, void **argv, void **envp, bool is_wchar);

pal_session_t *pal_opensession();
/* The returned cwd should not be free and not thread safe */
char *pal_session_getcwd(pal_session_t *pal);
/* This is not thread safe, caller should take care of thread safety */
int pal_session_chdir(pal_session_t *pal, const char *dirpath);
/* This is not thread safe, caller should take care of thread safety */
char *pal_session_realpath(pal_session_t *pal, const char *path);
/* This is not thread safe, caller should take care of thread safety */
void pal_closesession(pal_session_t *pal);

pal_session_t *pal_global();
void pal_finalize();

/* The returned cwd should be freed with pal_free and thread safe */
char *pal_getcwd();
int pal_chdir(const char *dirpath);

char *pal_strdup(const char *str);
void *pal_malloc(size_t sz);
void *pal_mallocz(size_t sz);
void pal_free(void *ptr);
pal_file_t pal_file_get(pal_file_type_t type);
pal_file_t pal_file_open_tmp();
int pal_open_flags(const char *mode, const char *expected);
pal_file_t pal_open(const char *filepath, int flags, pal_mode_t mode);
pal_file_t pal_popen(const char *process_command, int flags);
int pal_fsync(pal_file_t fd);
int pal_fgetc(pal_file_t fd);
int pal_fputc(int c, pal_file_t fd);
int pal_read(pal_file_t file, void *buffer, uint32_t sz);
int pal_write(pal_file_t file, const void *buffer, uint32_t sz);
int pal_writes(pal_file_t file, const char *str);
pal_off_t pal_lseek(pal_file_t file, pal_off_t off, int whence);
int pal_fstat(pal_file_t file, pal_stat_t *st);
int pal_stat(const char *path, pal_stat_t *st, int is_lstat);
int pal_eof(pal_file_t file);
int pal_close(pal_file_t file);
int pal_pclose(pal_file_t file);

/* Directory listing */
pal_dir_t *pal_opendir(const char *dirpath);
pal_dirent_t *pal_readdir(pal_dir_t *dirp);
int pal_closedir(pal_dir_t *dirp);
pal_d_type_t pal_iftodt(int ft);
int pal_dttoif(pal_d_type_t dt);
#define PAL_IFTODT pal_iftodt
#define PAL_DTTOIF pal_dttoif

/* Create a directory */
int pal_mkdir(const char *dirpath, pal_mode_t mode);
/* Create a directory with parent create if not exist */
int pal_mkdir_recursive(const char *dirpath, pal_mode_t mode);
/* Rename file or directory */
int pal_rename(const char *from, const char *to);
/* Remove a direction */
int pal_rmdir(const char *dirpath);
/* Remove a file */
int pal_unlink(const char *filepath);
/* Remove a file or directory */
int pal_remove(const char *path);
/* atime/utime: unit is ms */
int pal_utimes(const char *path, int64_t atime, int64_t mtime);
/* link_type: 0 means file, 1 means directory, -1 means detect from path1_target */
int pal_symlink(const char *path1_target, const char *path2_symlink, int link_type);
char *pal_readlink(const char *path);
int pal_pipe(int *pipe_handles, uint32_t pipe_size);
int pal_dup(pal_file_t fd);
int pal_dup2(pal_file_t fd1, pal_file_t fd2);

int pal_kill(pal_pid_t pid, int sig);
pal_pid_t pal_waitpid(pal_pid_t pid, int *stat_loc, int options);
int pal_setuid(pal_gid_t uid);
int pal_setgid(pal_uid_t gid);

/**
 * @brief execute subprocess
 * @return the pid of new subprocess
 */
pal_pid_t pal_execute(
    const char *file,
    const char *cwd,
    pal_process_info_t *info,
    bool use_path,
    int *std_fds, // [3]
    bool block_flag,
    pal_gid_t gid,
    pal_uid_t uid,
    int *exit_code);

/**
 * @brief return the canonicalized absolute pathname
 *   The result capacity should be able contain the full resolve_path
 *   and the trailing '\0'
 * @param is_absolute tell if result absolute path or result general(absolute or relative) path
 * @param base_path should be a absolute path if `is_absolute` is non-zero
 * @param resolved_path used to storage the allocaed resolved_path, if NULL it's will allocated
 *                    automatically, if non-NULL, it's should be allocated with pal_malloc
 * @param capacity use to input and output the capacity of resolved_path, the initial value should be
 *               non-zero
 * @return return the length of the `resolved_path`(not include the trailing '\0')
 */
int pal_joinpath(int is_absolute, const char *base_path, const char *from_path, char **resolved_path, int offset, int *capacity);
char *pal_realpath(const char *path);
char *pal_dirname(const char *path);

typedef int (*pal_listdir_callback_t)(void *context, const char *path, int is_dir);
int pal_listdir(void *context, const char *path, int recurse, pal_listdir_callback_t callback);

void *pal_dlopen(const char *filepath, int mode);
void *pal_dlsym(void *handle, const char *name);
int pal_dlclose(void *handle);

int pal_tty_isatty(pal_file_t fd);
int pal_tty_getwinsize(pal_file_t fd, int *width, int *height);
int pal_tty_setraw(pal_file_t fd);

/* creates a new event loop (defined by each backend) */
pal_ev_loop_t *pal_ev_create_loop(int max_timeout);

/* destroys a loop (defined by each backend) */
int pal_ev_destroy_loop(pal_ev_loop_t *loop);

/* internal: updates events to be watched (defined by each backend) */
int pal_ev_update_events_internal(pal_ev_loop_t *loop, int fd, int events);

/* internal: poll once and call the handlers (defined by each backend) */
int pal_ev_poll_once_internal(pal_ev_loop_t *loop, int max_wait);

/* internal, aligned allocator with address scrambling to avoid cache
     line contention */
void *pal_ev_memalign(size_t sz, void **orig_addr, int clear);

/* initializes pal_ev */
int pal_ev_init(int max_fd);

/* deinitializes pal_ev */
int pal_ev_deinit(void);

/* updates timeout */
void pal_ev_set_timeout(pal_ev_loop_t *loop, int fd, int64 usecs);

/* registers a file descriptor and callback argument to a event loop */
int pal_ev_add(pal_ev_loop_t *loop, int fd, int events, int timeout_in_secs,
               pal_ev_handler_t *callback, void *cb_arg);

/* unregisters a file descriptor from event loop */
int pal_ev_del(pal_ev_loop_t *loop, int fd);

/* check if fd is registered (checks all loops if loop == NULL) */
int pal_ev_is_active(pal_ev_loop_t *loop, int fd);

/* returns events being watched for given descriptor */
int pal_ev_get_events(pal_ev_loop_t *loop, int fd);

/* sets events to be watched for given desriptor */
int pal_ev_set_events(pal_ev_loop_t *loop, int fd, int events);

/* returns callback for given descriptor */
pal_ev_handler_t *pal_ev_get_callback(pal_ev_loop_t *loop,
                                      int fd, void **cb_arg);
/* sets callback for given descriptor */
void pal_ev_set_callback(pal_ev_loop_t *loop, int fd,
                         pal_ev_handler_t *callback, void **cb_arg);

/* function to iterate registered information. To start iteration, set curfd
     to -1 and call the function until -1 is returned */
int pal_ev_next_fd(pal_ev_loop_t *loop, int curfd);

/* internal function */
int pal_ev_init_loop_internal(pal_ev_loop_t *loop, int max_timeout);
/* internal function */
void pal_ev_deinit_loop_internal(pal_ev_loop_t *loop);

/* internal function */
void pal_ev_handle_timeout_internal(pal_ev_loop_t *loop);

/* loop once */
int pal_ev_loop_once(pal_ev_loop_t *loop, int max_wait);

#endif /* _PAL_PORT_H_ */
