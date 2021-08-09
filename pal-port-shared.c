
#include "cwalk.h"
#include "pal-port.h"

#include <fcntl.h>

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

int pal_writes(pal_file_t file, const char *str)
{
  return pal_write(file, str, (uint32_t)strlen(str));
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
