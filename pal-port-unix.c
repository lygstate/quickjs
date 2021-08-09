/*
 * QuickJS Port library
 *
 * Copyright (c) 2017-2021 Yonggang Luo
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/* Ensure we get the 64-bit variants of the CRT's file I/O calls */
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE 1
#endif
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <dirent.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/syslimits.h>
#endif

#include "pal-port.h"

char *pal_process_executable_path()
{
  /* https://stackoverflow.com/a/1024937 */
#if defined(__linux__) || defined(__FreeBSD__)
#ifdef __linux__
  char *filepath = pal_readlink("/proc/self/exe");
#else
  char *filepath = pal_readlink("/proc/curproc/file");
#endif
  char *abs_filepath = pal_realpath(filepath);
  pal_free(filepath);
  return abs_filepath;
#elif defined(__APPLE__)
  char path[1024];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0) {
    return pal_realpath(path);
  }
  return NULL;
#endif
}

char *pal_getenv(const char *name)
{
  return getenv(name);
}

int pal_setenv(const char *name, const char *value, int overwrite)
{
  return setenv(name, value, overwrite);
}

int pal_unsetenv(const char *name)
{
  return unsetenv(name);
}

int pal_clock_gettime(pal_clockid_t pal_clock_id, struct timespec *tp)
{
  clockid_t clock_id;
  switch (pal_clock_id) {
    PAL_TO_NATIVE_CASE(clock_id, CLOCK_REALTIME)
    PAL_TO_NATIVE_CASE(clock_id, CLOCK_MONOTONIC)
    PAL_TO_NATIVE_CASE(clock_id, CLOCK_PROCESS_CPUTIME_ID)
    PAL_TO_NATIVE_CASE(clock_id, CLOCK_THREAD_CPUTIME_ID)
#ifdef CLOCK_REALTIME_COARSE
    PAL_TO_NATIVE_CASE(clock_id, CLOCK_REALTIME_COARSE)
#endif
  default:
    errno = EINVAL;
    return -1;
  }
  return clock_gettime(clock_id, tp);
}

int pal_gettimezoneoffset(int64_t unix_ms, bool is_utc)
{
  struct tm tm;
  time_t now = (time_t)(unix_ms / 1000);
  localtime_r(&now, &tm);

  if (!is_utc) {
    now -= tm.tm_gmtoff;
    localtime_r(&now, &tm);
  }

  return tm.tm_gmtoff;
}

int pal_settimezoneoffset(int64_t time_ms, int timezone_offset)
{
  int timezone_offset_minutes = timezone_offset / 60;
  char tz_str[128];
  /* [GTM-12, GTM+14] is valid */
  if (timezone_offset_minutes >= 15 * 60) {
    return -1;
  }
  if (timezone_offset_minutes <= -13 * 60) {
    return -1;
  }
  if (timezone_offset_minutes == 0) {
    strcpy(tz_str, "GTM");
  } else if (timezone_offset_minutes > 0) {
    snprintf(tz_str, sizeof(tz_str), "UTC-%02d:%02d",
             timezone_offset_minutes / 60, timezone_offset_minutes % 60);
  } else {
    snprintf(tz_str, sizeof(tz_str), "UTC+%02d:%02d",
             -timezone_offset_minutes / 60, timezone_offset_minutes % 60);
  }
  pal_setenv("TZ", tz_str, 1);
}

int pal_msleep(int64_t ms)
{
  return pal_usleep(ms * 1000);
}

int pal_usleep(int64_t us)
{
  return usleep(us);
}

int pal_thread_create(pal_thread *thread, pal_thread_method method, void *data, int detached)
{
  int ret;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  /* no join at the end */
  if (detached) {
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  }
  ret = pthread_create((pthread_t *)thread, &attr, method, data);
  pthread_attr_destroy(&attr);
  return ret;
}

int pal_thread_join(pal_thread *thread)
{
  return pthread_join(*(pthread_t *)thread, NULL);
}

/* https://github.com/pierreguillot/thread */
int pal_mutex_init(pal_mutex *mutex)
{
  pthread_mutex_t mutex_initializer = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t *pmutex = (pthread_mutex_t *)pal_malloc(sizeof(pthread_mutex_t));
  *pmutex = mutex_initializer;
  *mutex = (pal_mutex)pmutex;
  return 0;
}

int pal_mutex_lock(pal_mutex *mutex)
{
  return pthread_mutex_lock((pthread_mutex_t *)*mutex);
}

int pal_mutex_trylock(pal_mutex *mutex)
{
  return pthread_mutex_trylock((pthread_mutex_t *)*mutex);
}

int pal_mutex_unlock(pal_mutex *mutex)
{
  return pthread_mutex_unlock((pthread_mutex_t *)*mutex);
}

int pal_mutex_destroy(pal_mutex *mutex)
{
  pal_free(*mutex);
  *mutex = NULL;
  return 0;
}

int pal_condition_init(pal_condition *cond)
{
  int ret = -1;
  pthread_cond_t *cond_ptr = (pthread_cond_t *)pal_malloc(sizeof(pthread_cond_t));
  if (cond_ptr == NULL) {
    goto done_cond;
  }
#if (defined(__APPLE__) && defined(__MACH__) || defined(__MVS__))
  ret = pthread_cond_init(cond_ptr, NULL);
#else  /* !((defined(__APPLE__) && defined(__MACH__) || defined(__MVS__))) */
  {
    pthread_condattr_t attr;
    ret = pthread_condattr_init(&attr);
    if (ret) {
      goto done_cond;
    }
    ret = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    if (ret) {
      goto done_attr;
    }
    ret = pthread_cond_init(cond_ptr, &attr);
  done_attr:
    pthread_condattr_destroy(&attr);
  }
#endif /* (defined(__APPLE__) && defined(__MACH__) || defined(__MVS__)) */

done_cond:
  if (ret) {
    pal_free(cond_ptr);
  } else {
    *cond = cond_ptr;
  }
  return ret;
}

int pal_condition_signal(pal_condition *cond)
{
  return pthread_cond_signal((pthread_cond_t *)*cond);
}

int pal_condition_broadcast(pal_condition *cond)
{
  return pthread_cond_broadcast((pthread_cond_t *)*cond);
}

int pal_condition_wait(pal_condition *cond, pal_mutex *mutex)
{
  return pthread_cond_wait((pthread_cond_t *)*cond, (pthread_mutex_t *)*mutex);
}

int pal_condition_timedwait(pal_condition *cond, pal_mutex *mutex, struct timespec *timeout)
{
  struct timespec abstime;
  int r;
#if defined(__APPLE__) && defined(__MACH__)
  r = pthread_cond_timedwait_relative_np((pthread_cond_t *)*cond, (pthread_mutex_t *)*mutex, timeout);
#else /* !(defined(__APPLE__) && defined(__MACH__)) */
#if defined(__MVS__)
  {
    struct timeval tv;
    if (gettimeofday(&tv, NULL))
      pal_abort();
    abstime.tv_sec = tv.tv_sec;
    abstime.tv_usec = tv.tv_usec * 1e3;
  }
#else
  /* CLOCK_MONOTONIC is based on how pal_condition are create by pal_condition_init */
  clock_gettime(CLOCK_MONOTONIC, &abstime);
#endif
  abstime.tv_sec += timeout->tv_sec;
  abstime.tv_nsec += timeout->tv_nsec;
  while (abstime.tv_nsec >= 1000000000) {
    abstime.tv_nsec -= 1000000000;
    abstime.tv_sec++;
  }
  r = pthread_cond_timedwait((pthread_cond_t *)*cond, (pthread_mutex_t *)*mutex, &abstime);
#endif /* defined(__APPLE__) && defined(__MACH__) */
  return r;
}

int pal_condition_destroy(pal_condition *cond)
{
#if defined(__APPLE__) && defined(__MACH__)
  /* It has been reported that destroying condition variables that have been
   * signalled but not waited on can sometimes result in application crashes.
   * See https://codereview.chromium.org/1323293005.
   */
  pthread_mutex_t mutex;
  struct timespec ts;
  int err;

  if (pthread_mutex_init(&mutex, NULL))
    pal_abort();

  if (pthread_mutex_lock(&mutex))
    pal_abort();

  ts.tv_sec = 0;
  ts.tv_nsec = 1;

  err = pthread_cond_timedwait_relative_np((pthread_cond_t *)*cond, &mutex, &ts);
  if (err != 0 && err != ETIMEDOUT)
    pal_abort();

  if (pthread_mutex_unlock(&mutex))
    pal_abort();

  if (pthread_mutex_destroy(&mutex))
    pal_abort();
#endif /* defined(__APPLE__) && defined(__MACH__) */
  {
    int result = pthread_cond_destroy((pthread_cond_t *)*cond);
    pal_free(*cond);
    *cond = NULL;
    return result;
  }
}

pal_dir_t *pal_opendir(const char *dirpath)
{
  DIR *native_dir = opendir(dirpath);
  pal_dir_t *dirp = NULL;
  if (native_dir != NULL) {
    dirp = (pal_dir_t *)pal_mallocz(sizeof(pal_dir_t));
    dirp->native = (pal_dir_native_t *)opendir(dirpath);
  }
  return dirp;
}

pal_dirent_t *pal_readdir(pal_dir_t *dirp)
{
  struct dirent *native = readdir((DIR *)dirp->native);
  if (native == NULL) {
    return NULL;
  }
  dirp->ent.d_ino = native->d_ino;
#if defined(__APPLE__)
  dirp->ent.d_off = native->d_seekoff;
#else
  dirp->ent.d_off = native->d_off;
#endif /* __APPLE__ */
  dirp->ent.d_reclen = sizeof(pal_dirent_t);
  dirp->ent.d_namlen = strlen(native->d_name);
  memcpy(dirp->ent.d_name, native->d_name, dirp->ent.d_namlen + 1);
  dirp->ent.d_type = native->d_type;

  dirp->ent.native = native;
  return &dirp->ent;
}

int pal_closedir(pal_dir_t *dirp)
{
  int ret = -1;
  if (dirp) {
    ret = closedir((DIR *)dirp->native);
    pal_free(dirp);
  } else {
    errno = EBADF;
  }
  return ret;
}

pal_d_type_t pal_iftodt(int ft)
{
  return IFTODT(ft);
}

int pal_dttoif(pal_d_type_t pal_dt)
{
  return DTTOIF(pal_dt);
}

static void pal_stat_from_native(pal_stat_t *st, struct stat *native_st)
{
  memset(st, 0, sizeof(*st));
  st->st_dev = native_st->st_dev;
  st->st_ino = native_st->st_ino;
  st->st_mode = native_st->st_mode;
  st->st_nlink = native_st->st_nlink;
  st->st_uid = native_st->st_uid;
  st->st_gid = native_st->st_gid;
  st->st_rdev = native_st->st_rdev;
  st->st_size = native_st->st_size;
  st->st_blksize = native_st->st_blksize;
  st->st_blocks = native_st->st_blocks;
#if defined(__APPLE__)
  st->st_atim = native_st->st_atimespec;
  st->st_mtim = native_st->st_mtimespec;
  st->st_ctim = native_st->st_ctimespec;
#else
  st->st_atim = native_st->st_atim;
  st->st_mtim = native_st->st_mtim;
  st->st_ctim = native_st->st_ctim;
#endif /* __APPLE__ */
}

pal_off_t pal_lseek(pal_file_t file, pal_off_t off, int whence)
{
  return lseek(file, off, whence);
}

int pal_fstat(pal_file_t file, pal_stat_t *st)
{
  struct stat native_st;
  int ret = fstat(file, &native_st);
  pal_stat_from_native(st, &native_st);
  return ret;
}

int pal_stat(const char *path, pal_stat_t *st, int is_lstat)
{
  struct stat native_st;
  int ret = -1;
  if (is_lstat) {
    ret = lstat(path, &native_st);
  } else {
    ret = stat(path, &native_st);
  }
  pal_stat_from_native(st, &native_st);
  return ret;
}

/* The returned cwd should be freed with pal_free and thread safe */
char *pal_getcwd()
{
  char *cwd = getcwd(NULL, 0);
  char *pal_cwd = NULL;
  if (cwd != NULL) {
    pal_cwd = pal_strdup(cwd);
    free(cwd);
  }
  return pal_cwd;
}

int pal_chdir(const char *dirpath)
{
  return chdir(dirpath);
}

void *pal_dlopen(const char *filepath, int mode)
{
  int mode_used = 0;
  if (mode & PAL_RTLD_LAZY) {
    mode_used |= RTLD_LAZY;
  }
  if (mode & PAL_RTLD_NOW) {
    mode_used |= RTLD_NOW;
  }
  if (mode & PAL_RTLD_NOLOAD) {
    mode_used |= RTLD_NOLOAD;
  }
#ifdef RTLD_DEEPBIND
  if (mode & PAL_RTLD_DEEPBIND) {
    mode_used |= RTLD_DEEPBIND;
  }
#endif
  if (mode & PAL_RTLD_GLOBAL) {
    mode_used |= RTLD_GLOBAL;
  }
  if (mode & PAL_RTLD_LOCAL) {
    mode_used |= RTLD_LOCAL;
  }
  if (mode & PAL_RTLD_NODELETE) {
    mode_used |= RTLD_NODELETE;
  }
  return dlopen(filepath, mode_used);
}

void *pal_dlsym(void *handle, const char *name)
{
  return dlsym(handle, name);
}

int pal_dlclose(void *handle)
{
  return dlclose(handle);
}

pal_file_t pal_open(const char *filepath, int flags, pal_mode_t mode)
{
  pal_file_t f = -1;
  if (mode > 0) {
    f = open(filepath, flags, mode);
  } else {
    f = open(filepath, flags);
  }
  return f;
}

int pal_fsync(pal_file_t fd)
{
  return fsync(fd);
}

int pal_pipe(int *pipe_handles, uint32_t pipe_size)
{
  return pipe(pipe_handles);
}

int pal_mkdir(const char *dirname, pal_mode_t mode)
{
  return mkdir(dirname, mode);
}

int pal_rename(const char *from, const char *to)
{
  return rename(from, to);
}

int pal_rmdir(const char *dirpath)
{
  return rmdir(dirpath);
}

int pal_unlink(const char *filepath)
{
  unlink(filepath);
}

int pal_remove(const char *path)
{
  return remove(path);
}

static void ms_to_timeval(struct timeval *tv, uint64_t v)
{
  tv->tv_sec = v / 1000;
  tv->tv_usec = (v % 1000) * 1000;
}

int pal_utimes(const char *path, int64_t atime, int64_t mtime)
{
  struct timeval times[2];
  ms_to_timeval(&times[0], atime);
  ms_to_timeval(&times[1], mtime);
  return utimes(path, times);
}

int pal_symlink(const char *path1_target, const char *path2_symlink, int link_type)
{
  return symlink(path1_target, path2_symlink);
}

char *pal_readlink(const char *path)
{
  struct stat sb;
  ssize_t r = INT_MAX;
  int linkSize = 0;
  const int growthRate = 255;

  char *linkTarget = NULL;

  /* get length of the pathname the link points to
     could not lstat: insufficient permissions on directory? */
  if (lstat(path, &sb) == -1) {
    return NULL;
  }

  if (!S_ISLNK(sb.st_mode)) {
    return NULL;
  }

  /* read the link target into a string */
  linkSize = sb.st_size + 1;
  for (;;) {
    /* i.e. symlink increased in size since lstat() or non-POSIX compliant filesystem
         allocate sufficient memory to hold the link */
    linkTarget = pal_malloc(linkSize);
    /* insufficient memory */
    if (linkTarget == NULL) {
      return NULL;
    }

    /* read the link target into variable linkTarget */
    r = readlink(path, linkTarget, linkSize);
    if (r >= 0 && r < linkSize) {
      /* Succeed: readlink does not null-terminate the string */
      linkTarget[r] = '\0';
      break;
    } else {
      pal_free(linkTarget);
      linkTarget = NULL;
      /* readlink failed: link was deleted? */
      if (r < 0) {
        break;
      } else {
        /* linkSize too small, increase it */
        linkSize += growthRate;
        r = linkSize;
      }
    }
  }
  return linkTarget;
}

int pal_kill(pal_pid_t pid, int sig)
{
  return kill(pid, sig);
}

pal_pid_t pal_waitpid(pal_pid_t pid, int *stat_loc, int options)
{
  return waitpid(pid, stat_loc, options);
}

int pal_setuid(uint32_t uid)
{
  return setuid(uid);
}

int pal_setgid(uint32_t gid)
{
  return setgid(gid);
}

/* execvpe is not available on non GNU systems */
static int my_execvpe(const char *filename, char **argv, char **envp)
{
  char *path, *p, *p_next, *p1;
  char buf[PAL_PATH_MAX];
  size_t filename_len, path_len;
  bool eacces_error;

  filename_len = strlen(filename);
  if (filename_len == 0) {
    errno = ENOENT;
    return -1;
  }
  if (strchr(filename, '/'))
    return execve(filename, argv, envp);

  path = getenv("PATH");
  if (!path)
    path = (char *)"/bin:/usr/bin";
  eacces_error = false;
  p = path;
  for (p = path; p != NULL; p = p_next) {
    p1 = strchr(p, ':');
    if (!p1) {
      p_next = NULL;
      path_len = strlen(p);
    } else {
      p_next = p1 + 1;
      path_len = p1 - p;
    }
    /* path too long */
    if ((path_len + 1 + filename_len + 1) > PAL_PATH_MAX)
      continue;
    memcpy(buf, p, path_len);
    buf[path_len] = '/';
    memcpy(buf + path_len + 1, filename, filename_len);
    buf[path_len + 1 + filename_len] = '\0';

    execve(buf, argv, envp);

    switch (errno) {
    case EACCES:
      eacces_error = true;
      break;
    case ENOENT:
    case ENOTDIR:
      break;
    default:
      return -1;
    }
  }
  if (eacces_error)
    errno = EACCES;
  return -1;
}

pal_pid_t pal_execute(
    const char *file,
    const char *cwd,
    pal_process_info_t *info,
    bool use_path,
    int *std_fds, // [3]
    bool block_flag,
    pal_gid_t gid,
    pal_uid_t uid,
    int *exit_code)
{
  int i;
  int status;
  int32_t ret;
  pal_pid_t pid;
  if (!file)
    file = info->argv[0];
  pid = fork();
  *exit_code = 0;
  if (pid < 0) {
    return pid;
  }
  if (pid == 0) {
    /* child */
    int fd_max = sysconf(_SC_OPEN_MAX);

    /* remap the stdin/stdout/stderr handles if necessary */
    for (i = 0; i < 3; i++) {
      if (std_fds[i] != i) {
        if (dup2(std_fds[i], i) < 0)
          pal_exit(127);
      }
    }

    for (i = 3; i < fd_max; i++)
      pal_close(i);
    if (cwd) {
      if (pal_chdir(cwd) < 0)
        pal_exit(127);
    }
    if (uid != -1) {
      if (pal_setuid(uid) < 0)
        pal_exit(127);
    }
    if (gid != -1) {
      if (pal_setgid(gid) < 0)
        pal_exit(127);
    }

    if (use_path)
      ret = my_execvpe(file, (char **)info->argv, info->envp);
    else
      ret = execve(file, (char **)info->argv, info->envp);
    pal_exit(127);
  }
  /* parent */
  if (block_flag) {
    int child_exit_code = 0;
    for (;;) {
      ret = pal_waitpid(pid, &status, 0);
      if (ret == pid) {
        if (WIFEXITED(status)) {
          child_exit_code = WEXITSTATUS(status);
          break;
        } else if (WIFSIGNALED(status)) {
          child_exit_code = -WTERMSIG(status);
          break;
        }
      }
    }
    *exit_code = child_exit_code;
  }
  return pid;
}

int pal_tty_getwinsize(pal_file_t fd, int *width, int *height)
{
  int ret = -1;
  struct winsize ws;
  if (ioctl(fd, TIOCGWINSZ, &ws) == 0) {
    if (ws.ws_col >= 4 && ws.ws_row >= 4) {
      *width = ws.ws_col;
      *height = ws.ws_row;
      ret = 0;
    } else {
      errno = ERANGE;
    }
  }
  return ret;
}

static struct termios oldtty;
static void term_exit(void)
{
  tcsetattr(0, TCSANOW, &oldtty);
}
int pal_tty_setraw(pal_file_t fd)
{
  struct termios tty;

  memset(&tty, 0, sizeof(tty));
  tcgetattr(fd, &tty);
  oldtty = tty;

  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  tty.c_oflag |= OPOST;
  tty.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
  tty.c_cflag &= ~(CSIZE | PARENB);
  tty.c_cflag |= CS8;
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 0;

  tcsetattr(fd, TCSANOW, &tty);

  atexit(term_exit);
  return 0;
}
