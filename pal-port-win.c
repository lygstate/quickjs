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

#include <io.h>
#include <sys/utime.h>
#include <windows.h>

#include "cutils.h"
#include "pal-port.h"

typedef pal_dir_t DIR;
typedef pal_dirent_t dirent_t;
#define DT_CHR PAL_DT_CHR
#define DT_DIR PAL_DT_DIR
#define DT_REG PAL_DT_REG
#define DT_UNKNOWN PAL_DT_UNKNOWN
#include "win/dirent.h"

wchar_t *pal_wpath(const char *cpath)
{
    wchar_t *wpath = NULL;
    size_t n;
    errno = 0;
    if (cpath != NULL) {
        int error = dirent_mbstowcs_s(&n, NULL, 0, cpath, SIZE_MAX);
        if (error == 0) {
            wpath = pal_malloc(sizeof(wchar_t) * (n));
            if (wpath != NULL) {
                error = dirent_mbstowcs_s(&n, wpath, n, cpath, SIZE_MAX);
                if (error != 0) {
                    pal_free(wpath);
                    wpath = NULL;
                }
            }
        }
    }
    return wpath;
}

char *pal_cpath(const wchar_t *wpath)
{
    char *cpath = NULL;
    size_t n;
    errno = 0;
    if (wpath != NULL) {
        int error = dirent_wcstombs_s(&n, NULL, 0, wpath, SIZE_MAX);
        if (error == 0) {
            cpath = (char *)pal_malloc(n);
            if (cpath != NULL) {
                error = dirent_wcstombs_s(&n, cpath, n, wpath, SIZE_MAX);
                if (error != 0) {
                    pal_free(cpath);
                    cpath = NULL;
                }
            }
        }
    }
    return cpath;
}

static void pal_file_time_to_timespec(FILETIME file_time, struct timespec *tp)
{
    static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);
    uint64_t time;
    time = ((uint64_t)file_time.dwLowDateTime);
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec = (int64_t)((time - EPOCH) / 10000000L);
    tp->tv_nsec = (time % 10000000L) * 100;
}

/* TODO: Use  QueryPerformanceCounter/GetTickCount for PAL_CLOCK_MONOTONIC */
int pal_clock_gettime(pal_clockid_t clock_id, struct timespec *tp)
{
    FILETIME file_time;
    GetSystemTimeAsFileTime(&file_time);
    pal_file_time_to_timespec(file_time, tp);
    return 0;
}

static const LONGLONG UnixEpochInTicks = 116444736000000000LL; /* difference between 1970 and 1601 */
static const LONGLONG TicksPerMs = 10000LL;                    /* 1 tick is 100 nanoseconds */

/*
 * If you take the limit of SYSTEMTIME (last millisecond in 30827) then you end up with
 * a FILETIME of 0x7fff35f4f06c58f0 by using SystemTimeToFileTime(). However, if you put
 * 0x7fffffffffffffff into FileTimeToSystemTime() then you will end up in the year 30828,
 * although this date is invalid for SYSTEMTIME. Any larger value (0x8000000000000000 and above)
 * causes FileTimeToSystemTime() to fail.
 * https://docs.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-systemtime
 * https://docs.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-filetime
 */
static const LONGLONG UnixEpochOfDate_1601_01_02 = -11644387200000LL;   /* unit: ms */
static const LONGLONG UnixEpochOfDate_30827_12_29 = 9106702560000000LL; /* unit: ms */

/* https://support.microsoft.com/en-us/help/167296/how-to-convert-a-unix-time-t-to-a-win32-filetime-or-systemtime */
static void UnixTimeMsToFileTime(double t, LPFILETIME pft)
{
    LONGLONG ll = (LONGLONG)t * TicksPerMs + UnixEpochInTicks;
    pft->dwLowDateTime = (DWORD)ll;
    pft->dwHighDateTime = (DWORD)(ll >> 32);
} /* UnixTimeMsToFileTime */

int pal_gettimezoneoffset(int64_t time)
{
    FILETIME utcFileTime, localFileTime;
    SYSTEMTIME utcSystemTime, localSystemTime;
    int timeConverted = 0;

    /*
    * If the time is earlier than the date 1601-01-02, then always using date 1601-01-02 to
    * query time zone adjustment. This date (1601-01-02) will make sure both UTC and local
    * time succeed with Win32 API. The date 1601-01-01 may lead to a win32 api failure, as
    * after converting between local time and utc time, the time may be earlier than 1601-01-01
    * in UTC time, that exceeds the FILETIME representation range.
    */
    if (time < (double)UnixEpochOfDate_1601_01_02) {
        time = (double)UnixEpochOfDate_1601_01_02;
    }

    /* Like above, do not use the last supported day */
    if (time > (double)UnixEpochOfDate_30827_12_29) {
        time = (double)UnixEpochOfDate_30827_12_29;
    }

    UnixTimeMsToFileTime(time, &utcFileTime);
    if (FileTimeToSystemTime(&utcFileTime, &utcSystemTime) && SystemTimeToTzSpecificLocalTime(0, &utcSystemTime, &localSystemTime) && SystemTimeToFileTime(&localSystemTime, &localFileTime)) {
        timeConverted = 1;
    }
    if (timeConverted) {
        ULARGE_INTEGER utcTime, localTime;
        utcTime.LowPart = utcFileTime.dwLowDateTime;
        utcTime.HighPart = utcFileTime.dwHighDateTime;
        localTime.LowPart = localFileTime.dwLowDateTime;
        localTime.HighPart = localFileTime.dwHighDateTime;
        return (int)(((LONGLONG)localTime.QuadPart - (LONGLONG)utcTime.QuadPart) / TicksPerMs);
    }
    return 0.0;
}

int pal_msleep(int64_t ms)
{
    Sleep(ms);
    return 0;
}

int pal_usleep(int64_t us)
{
    Sleep(us / 1000);
    return 0;
}

typedef struct _internal_parameters {
    pal_thread_method i_method;
    void *i_data;
} t_internal_parameters;

static DWORD WINAPI internal_method_ptr(LPVOID arg)
{
    t_internal_parameters *params = (t_internal_parameters *)arg;
    params->i_method(params->i_data);
    pal_free(params);
    return 0;
}

int pal_thread_create(pal_thread *thread, pal_thread_method method, void *data, int detached)
{
    t_internal_parameters *params = (t_internal_parameters *)pal_malloc(sizeof(t_internal_parameters));
    if (params) {
        params->i_method = method;
        params->i_data = data;
        *thread = CreateThread(NULL, 0, internal_method_ptr, params, 0, NULL);
        if (*thread == NULL) {
            pal_free(params);
            return 1;
        }
        return 0;
    }
    return 1;
}

int pal_thread_join(pal_thread *thread)
{
    if (WaitForSingleObject(*thread, INFINITE) != WAIT_FAILED) {
        if (CloseHandle(*thread)) {
            return 0;
        }
    }
    return 1;
}

/* https://github.com/pierreguillot/thread */
int pal_mutex_init(pal_mutex *mutex)
{
    *mutex = pal_malloc(sizeof(CRITICAL_SECTION));
    InitializeCriticalSection((CRITICAL_SECTION *)*mutex);
    return 0;
}

int pal_mutex_lock(pal_mutex *mutex)
{
    EnterCriticalSection((CRITICAL_SECTION *)*mutex);
    return 0;
}

int pal_mutex_trylock(pal_mutex *mutex)
{
    return !TryEnterCriticalSection((CRITICAL_SECTION *)*mutex);
}

int pal_mutex_unlock(pal_mutex *mutex)
{
    LeaveCriticalSection((CRITICAL_SECTION *)*mutex);
    return 0;
}

int pal_mutex_destroy(pal_mutex *mutex)
{
    DeleteCriticalSection((CRITICAL_SECTION *)*mutex);
    pal_free(*mutex);
    *mutex = NULL;
    return 0;
}

int pal_condition_init(pal_condition *cond)
{
    *cond = 0;
    InitializeConditionVariable((CONDITION_VARIABLE *)cond);
    return 0;
}

int pal_condition_signal(pal_condition *cond)
{
    WakeConditionVariable((CONDITION_VARIABLE *)cond);
    return 0;
}

int pal_condition_broadcast(pal_condition *cond)
{
    WakeAllConditionVariable((CONDITION_VARIABLE *)cond);
    return 0;
}

int pal_condition_wait(pal_condition *cond, pal_mutex *mutex)
{
    if (SleepConditionVariableCS((CONDITION_VARIABLE *)cond, (CRITICAL_SECTION *)*mutex, INFINITE)) {
        return 0;
    }
    errno = EINVAL;
    return errno;
}

int pal_condition_timedwait(pal_condition *cond, pal_mutex *mutex, struct timespec *timeout)
{
    int64_t time_ms = timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000;
    if ((timeout->tv_nsec % 1000000) != 0) {
        time_ms += 1;
    }
    if (SleepConditionVariableCS((CONDITION_VARIABLE *)cond, (CRITICAL_SECTION *)*mutex, time_ms)) {
        return 0;
    }
    if (GetLastError() == ERROR_TIMEOUT) {
        errno = ETIMEDOUT;
    } else {
        errno = EINVAL;
    }
    return errno;
}

int pal_condition_destroy(pal_condition *cond)
{
    *cond = 0;
    return 0;
}

pal_dir_t *pal_opendir(const char *dirpath)
{
    return opendir(dirpath);
}

pal_dirent_t *pal_readdir(pal_dir_t *dir)
{
    return readdir(dir);
}

int pal_closedir(pal_dir_t *dir)
{
    if (dir) {
        return closedir(dir);
    }
    errno = EINVAL;
    return -1;
}

#define PAL_IF_TO_DT_MAP(name) \
    case S_IF##name:           \
        return PAL_DT_##name

pal_d_type_t pal_iftodt(int ft)
{
    int dt = (ft)&S_IFMT;
    switch (dt) {
    case S_IFREG:
        return PAL_DT_REG;
    case S_IFDIR:
        return PAL_DT_DIR;
    case S_IFCHR:
        return PAL_DT_CHR;
    case S_IFIFO:
        return PAL_DT_FIFO;
    default:
        return PAL_DT_UNKNOWN;
    }
}

int pal_dttoif(pal_d_type_t pal_dt)
{
    switch (pal_dt) {
    case PAL_DT_REG:
        return S_IFREG;
    case PAL_DT_DIR:
        return S_IFDIR;
    case PAL_DT_FIFO:
        return S_IFIFO;
    case PAL_DT_CHR:
        return S_IFCHR;
    }
    return 0;
}

static void pal_stat_from_native(pal_stat_t *st, struct _stat64 *native_st)
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
    /* TODO: st_blksize st_blocks */
    st->st_atim.tv_sec = native_st->st_atime;
    st->st_mtim.tv_sec = native_st->st_mtime;
    st->st_ctim.tv_sec = native_st->st_ctime;
}

pal_off_t pal_lseek(pal_file_t file, pal_off_t off, int whence)
{
    return _lseeki64(file, off, whence);
}

int pal_fstat_win32(HANDLE hFile, pal_stat_t *st, int is_lstat)
{
    BY_HANDLE_FILE_INFORMATION fiFileInfo;
    DWORD attr;
    memset(st, 0, sizeof(*st));
    if (hFile == INVALID_HANDLE_VALUE) {
        errno = EIO;
        return -1;
    }
    if (!GetFileInformationByHandle(hFile, &fiFileInfo)) {
        errno = EIO;
        return -1;
    }
    st->st_dev = fiFileInfo.dwVolumeSerialNumber;
    st->st_ino = (uint64_t)fiFileInfo.nFileIndexHigh << 32 | fiFileInfo.nFileIndexLow;
    st->st_nlink = fiFileInfo.nNumberOfLinks;
    attr = fiFileInfo.dwFileAttributes;
    st->st_mode = 0;
    if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
        if (is_lstat) {
            st->st_mode = S_IFLNK;
        }
    }
    if (st->st_mode == 0)
        if ((attr & FILE_ATTRIBUTE_DEVICE) != 0) {
            st->st_mode = S_IFCHR;
        } else if ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            st->st_mode = S_IFDIR;
        } else {
            st->st_mode = S_IFREG;
        }
    st->st_size = (uint64_t)fiFileInfo.nFileSizeHigh << 32 | fiFileInfo.nFileSizeLow;
    pal_file_time_to_timespec(fiFileInfo.ftCreationTime, &st->st_ctim);
    pal_file_time_to_timespec(fiFileInfo.ftLastWriteTime, &st->st_mtim);
    pal_file_time_to_timespec(fiFileInfo.ftLastAccessTime, &st->st_atim);
    return 0;
}

int pal_fstat(pal_file_t file, pal_stat_t *st)
{
    return pal_fstat_win32((HANDLE)_get_osfhandle(file), st, 0);
}

static int pal_wstat(const wchar_t *wpath, pal_stat_t *st, int is_lstat)
{
    int ret;
    HANDLE hFile;
    DWORD fileFlags = FILE_FLAG_BACKUP_SEMANTICS;
    if (is_lstat) {
        fileFlags |= FILE_FLAG_OPEN_REPARSE_POINT;
    }
    hFile = CreateFileW(
        wpath,
        GENERIC_READ,
        FILE_SHARE_READ, /* FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRIT */
        NULL,
        OPEN_EXISTING,
        fileFlags,
        NULL);
    ret = pal_fstat_win32(hFile, st, is_lstat);
    CloseHandle(hFile);
    return ret;
}

int pal_stat(const char *path, pal_stat_t *st, int is_lstat)
{
    int ret = -1;
    wchar_t *wpath = pal_wpath(path);
    if (wpath != NULL) {
        ret = pal_wstat(wpath, st, is_lstat);
        pal_free(wpath);
    }
    return ret;
}

char *pal_getcwd()
{
    char *cwd = NULL;
    wchar_t *wcwd = _wgetcwd(NULL, 0);
    if (wcwd != NULL) {
        cwd = pal_cpath(wcwd);
        free(wcwd);
        wcwd = NULL;
    }
    return cwd;
}

int pal_chdir(const char *dirpath)
{
    int ret = -1;
    wchar_t *wdirpath = pal_wpath(dirpath);
    if (wdirpath != NULL) {
        ret = _wchdir(wdirpath);
        pal_free(wdirpath);
    }
    return ret;
}

void *pal_dlopen(const char *filepath, int mode)
{
    void *result = NULL;
    wchar_t *wfilepath = pal_wpath(filepath);
    if (wfilepath != NULL) {
        result = LoadLibraryW(wfilepath);
        pal_free(wfilepath);
    }
    return result;
}

void *pal_dlsym(void *handle, const char *name)
{
    return (void *)GetProcAddress((HMODULE)handle, name);
}

int pal_dlclose(void *handle)
{
    if (CloseHandle((HANDLE)handle)) {
        errno = 0;
        return 0;
    }
    errno = EINVAL;
    return -1;
}

static int pal_create_pmode(int flags)
{
    if (flags & O_CREAT) {
        if (flags & O_RDWR) {
            return _S_IREAD | _S_IWRITE;
        } else if (flags & O_WRONLY) {
            return _S_IWRITE;
        }
        return _S_IREAD;
    }
    return -1;
}

pal_file_t pal_open(const char *filepath, int flags, pal_mode_t mode)
{
    pal_file_t f = -1;
    wchar_t *wfilepath = pal_wpath(filepath);
    if (wfilepath != NULL) {
        if (mode != -1) {
            f = _wopen(wfilepath, flags, mode);
        } else {
            int pmode = pal_create_pmode(flags);
            if (pmode == -1) {
                f = _wopen(wfilepath, flags);
            } else {
                f = _wopen(wfilepath, flags, pmode);
            }
        }
        pal_free(wfilepath);
    }
    if (f >= 0) {
        /* always force binary mode, for cross-platform consistence */
        _setmode(f, _O_BINARY);
    }
    return f;
}

int pal_fsync(pal_file_t fd)
{
    return 0;
}

int pal_pipe(int *pipe_handles, uint32_t pipe_size)
{
    errno = 0;
    return _pipe(pipe_handles, pipe_size, O_BINARY);
}

int pal_mkdir(const char *dirpath, int mode)
{
    int ret = -1;
    wchar_t *wdirpath = pal_wpath(dirpath);
    if (wdirpath != NULL) {
        ret = _wmkdir(wdirpath);
        pal_free(wdirpath);
    }
    return ret;
}

/* Rename file or directory */
int pal_rename(const char *from, const char *to)
{
    int ret = -1;
    wchar_t *wpath_from = pal_wpath(from);
    wchar_t *wpath_to = pal_wpath(to);
    if (wpath_from != NULL && wpath_to != NULL) {
        ret = _wrename(wpath_from, wpath_to);
    }
    pal_free(wpath_from);
    pal_free(wpath_to);
    return ret;
}

/* Remove direction */
int pal_rmdir(const char *dirpath)
{
    int ret = 1;
    wchar_t *wdirpath = pal_wpath(dirpath);
    if (wdirpath) {
        ret = _wrmdir(wdirpath);
        pal_free(wdirpath);
    }
    return ret;
}

/* Remove a file */
int pal_unlink(const char *filepath)
{
    int ret = 1;
    wchar_t *wfilepath = pal_wpath(filepath);
    if (wfilepath) {
        ret = _wunlink(wfilepath);
        pal_free(wfilepath);
    }
    return ret;
}

int pal_remove(const char *path)
{
    int ret = -1;
    wchar_t *wpath = pal_wpath(path);
    if (wpath != NULL) {
        pal_stat_t st;
        if (pal_wstat(wpath, &st, 1) == 0) {
            if (S_ISLNK(st.st_mode)) {
                if (DeleteFileW(wpath)) {
                    ret = 0;
                } else {
                    errno = EIO;
                }
            } else if (S_ISREG(st.st_mode)) {
                if (DeleteFileW(wpath)) {
                    ret = 0;
                } else {
                    errno = EIO;
                }
            } else {
                if (RemoveDirectoryW(wpath)) {
                    ret = 0;
                } else {
                    errno = EIO;
                }
            }
        }

        pal_free(wpath);
    }
    return ret;
}

int pal_utimes(const char *path, int64_t atime, int64_t mtime)
{
    int ret = -1;
    wchar_t *wpath = pal_wpath(path);
    if (wpath != NULL) {
        struct __utimbuf64 times;
        times.actime = atime / 1000;
        times.modtime = mtime / 1000;
        ret = _wutime64(wpath, &times);
        pal_free(wpath);
    }
    return ret;
}

int pal_symlink(const char *path1_target, const char *path2_symlink, int link_type)
{
    int ret = -1;
    wchar_t *wpath1_target = pal_wpath(path1_target);
    wchar_t *wpath2_symlink = pal_wpath(path2_symlink);
    if (wpath1_target != NULL && wpath2_symlink != NULL) {
        int is_dir = 0;
        DWORD dwFlags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
        if (link_type == 1) {
            is_dir = 1;
        } else if (link_type < 0) {
            struct _stat64 st;
            _wstat64(wpath1_target, &st);
            if (S_ISDIR(st.st_mode)) {
                is_dir = 1;
            }
        }
        if (is_dir == 1) {
            dwFlags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
        }
        if (CreateSymbolicLinkW(wpath2_symlink, wpath1_target, dwFlags)) {
            ret = 0;
        } else {
            errno = ENOTSUP;
        }
    }
    pal_free(wpath1_target);
    pal_free(wpath2_symlink);
    return ret;
}

#define REPARSE_DATA_BUFFER_HEADER_SIZE FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer)

typedef struct _REPARSE_DATA_BUFFER {
    ULONG ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union {
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG Flags; // it seems that the docu is missing this entry (at least 2008-03-07)
            wchar_t PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            wchar_t PathBuffer[1];
        } MountPointReparseBuffer;
        struct {
            UCHAR DataBuffer[1];
        } GenericReparseBuffer;
    };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

char *pal_readlink(const char *path)
{
    wchar_t *wpath = pal_wpath(path);
    wchar_t *wlinkpath = NULL;
    char *clinkpath = NULL;
    if (wpath != NULL) {
        HANDLE hPath = CreateFileW(
            wpath, FILE_READ_EA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
        pal_free(wpath);
        wpath = NULL;
        if (hPath != INVALID_HANDLE_VALUE) {
            /* Allocate the reparse data structure */
            DWORD dwBufSize = MAXIMUM_REPARSE_DATA_BUFFER_SIZE;
            REPARSE_DATA_BUFFER *rdata;
            rdata = (REPARSE_DATA_BUFFER *)pal_malloc(dwBufSize);
            /*Query the reparse data */
            DWORD dwRetLen;
            BOOL bRet = DeviceIoControl(hPath, FSCTL_GET_REPARSE_POINT, NULL, 0,
                                        rdata, dwBufSize, &dwRetLen, NULL);
            if (IsReparseTagMicrosoft(rdata->ReparseTag)) {
                if (rdata->ReparseTag == IO_REPARSE_TAG_SYMLINK) {
                    size_t path_buffer_offset = rdata->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(wchar_t);
                    size_t slen = rdata->SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(wchar_t);
                    wlinkpath = pal_malloc(sizeof(wchar_t) * (slen + 1));
                    memcpy(wlinkpath, &rdata->SymbolicLinkReparseBuffer.PathBuffer[path_buffer_offset], slen * sizeof(wchar_t));
                    wlinkpath[slen] = 0;
                } else if (rdata->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT) {
                    size_t path_buffer_offset = rdata->MountPointReparseBuffer.SubstituteNameOffset / sizeof(wchar_t);
                    size_t slen = rdata->MountPointReparseBuffer.SubstituteNameLength / sizeof(wchar_t);
                    wlinkpath = pal_malloc(sizeof(wchar_t) * (slen + 1));
                    memcpy(wlinkpath, &rdata->MountPointReparseBuffer.PathBuffer[path_buffer_offset], slen * sizeof(wchar_t));
                    wlinkpath[slen] = 0;
                } else {
                    errno = EIO;
                }
            } else {
                errno = EINVAL;
            }
            pal_free(rdata);
            CloseHandle(hPath);
        } else {
            errno = EEXIST;
        }
    }
    if (wlinkpath != NULL) {
        clinkpath = pal_cpath(wlinkpath);
        pal_free(wlinkpath);
        wlinkpath = NULL;
    }
    return clinkpath;
}

int pal_kill(pal_pid_t pid, int sig)
{
    errno = ENOTSUP;
    return -1;
}

int pal_tty_getwinsize(pal_file_t fd, int *width, int *height)
{
    int ret = -1;
    HANDLE handle = (HANDLE)_get_osfhandle(fd);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(handle, &info)) {
        *width = info.dwSize.X;
        *height = info.dwSize.Y;
        ret = 0;
    } else {
        errno = EINVAL;
    }
    return ret;
}

pal_pid_t pal_waitpid(pal_pid_t pid, int *stat_loc, int options)
{
#if defined(_WIN32)
    errno = ENOTSUP;
    return -1;
#else
    return waitpid(pid, stat_loc, options);
#endif
}

int pal_setuid(pal_uid_t uid)
{
    errno = ENOTSUP;
    return -1;
}

int pal_setgid(pal_gid_t gid)
{
    errno = ENOTSUP;
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
    errno = ENOTSUP;
    return -1;
}

/* Windows 10 built-in VT100 emulation */
#define __ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#define __ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
int pal_tty_setraw(pal_file_t fd)
{
    HANDLE handle = (HANDLE)_get_osfhandle(fd);
    SetConsoleMode(handle, ENABLE_WINDOW_INPUT | __ENABLE_VIRTUAL_TERMINAL_INPUT);
    _setmode(fd, _O_BINARY);
    if (fd == 0) {
        handle = (HANDLE)_get_osfhandle(1); /* corresponding output */
        SetConsoleMode(handle, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | __ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    return 0;
}