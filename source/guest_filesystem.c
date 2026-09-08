#include "../include/guest_filesystem.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define GUEST_PATH_MAX 4096

// Normalizes names within the guest root
static bool normalize_guest_path(const char *path, char *result) {
  size_t length = 0;
  if (path == NULL || *path == '\0') {
    errno = ENOENT;
    return false;
  }
  while (*path != '\0') {
    if (*path == '/') {
      path++;
      continue;
    }
    const char *segment = path;
    while (*path != '\0' && *path != '/') {
      // Rejects host drive names, alternate streams and Windows separators
      if (*path == '\\' || *path == ':') {
        errno = EACCES;
        return false;
      }
      path++;
    }
    size_t size = path - segment;
    if (size == 1 && segment[0] == '.') {
      continue;
    }
    if (size == 2 && segment[0] == '.' && segment[1] == '.') {
      while (length > 0 && result[length - 1] != '/') {
        length--;
      }
      if (length > 0) {
        length--;
      }
      continue;
    }
    if (length + size + 2 > GUEST_PATH_MAX) {
      errno = ENAMETOOLONG;
      return false;
    }
    if (length != 0) {
      result[length++] = '/';
    }
    memcpy(result + length, segment, size);
    length += size;
  }
  result[length] = '\0';
  return true;
}

typedef struct {
  char **entries;
  size_t count;
} DirectoryList;

static bool add_directory_entry(DirectoryList *list, const char *name,
                                bool directory) {
  char *entry = malloc(strlen(name) + 4);
  if (entry == NULL) {
    return false;
  }
  sprintf(entry, "%c %s\n", directory ? 'd' : '-', name);
  char **entries = realloc(list->entries, (list->count + 1) * sizeof(*entries));
  if (entries == NULL) {
    free(entry);
    return false;
  }
  list->entries = entries;
  list->entries[list->count++] = entry;
  return true;
}

#ifdef _WIN32
#include "guest_filesystem_windows.inc"
#else
#include <dirent.h>
#include <time.h>
#ifdef __linux__
#include <linux/openat2.h>
#include <sys/syscall.h>
#endif

static int root_fd = -1;

void close_guest_filesystem(void) {
  if (root_fd >= 0) {
    close(root_fd);
  }
  root_fd = -1;
}

bool init_guest_filesystem(void) {
  close_guest_filesystem();
  root_fd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  return root_fd >= 0;
}

// Pins each directory and never follows symbolic links on POSIX hosts
static int open_beneath(int root, const char *path, int flags, mode_t mode) {
#ifdef __linux__
  struct open_how how = {
      .flags = flags | O_NOFOLLOW | O_CLOEXEC,
      .mode = mode,
      .resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_XDEV,
  };
  int result = syscall(SYS_openat2, root, path, &how, sizeof(how));
  if (result >= 0 || errno != ENOSYS) {
    return result;
  }
#endif
  char copy[GUEST_PATH_MAX];
  strcpy(copy, path);
  int parent = dup(root);
  if (parent < 0) {
    return -1;
  }
  char *name = copy;
  char *slash;
  while ((slash = strchr(name, '/')) != NULL) {
    *slash = '\0';
    int next =
        openat(parent, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    close(parent);
    if (next < 0) {
      return -1;
    }
    parent = next;
    name = slash + 1;
  }
  int fd = openat(parent, name, flags | O_NOFOLLOW | O_CLOEXEC, mode);
  int saved_errno = errno;
  close(parent);
  errno = saved_errno;
  return fd;
}

static int open_guest_path(const char *path, int flags, mode_t mode) {
  char relative[GUEST_PATH_MAX];
  if (!normalize_guest_path(path, relative)) {
    return -1;
  }
  return open_beneath(root_fd, *relative == '\0' ? "." : relative, flags, mode);
}

FILE *guest_fopen(const char *path, const char *mode) {
  int flags = strchr(mode, '+') != NULL ? O_RDWR
              : *mode == 'r'            ? O_RDONLY
                                        : O_WRONLY;
  if (*mode == 'w' || *mode == 'a') {
    flags |= O_CREAT;
  }
  if (*mode == 'a') {
    flags |= O_APPEND;
  }
  int fd =
      open_guest_path(path, flags | O_NONBLOCK, flags & O_CREAT ? 0666 : 0);
  if (fd < 0) {
    return NULL;
  }
  struct stat st;
  // Rejects special files and hard links before truncating the opened file
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1) {
    close(fd);
    errno = EACCES;
    return NULL;
  }
  if (*mode == 'w' && ftruncate(fd, 0) != 0) {
    close(fd);
    return NULL;
  }
  FILE *file = fdopen(fd, mode);
  if (file == NULL) {
    close(fd);
  }
  return file;
}

bool guest_is_directory(const char *path) {
  int fd = open_guest_path(path, O_RDONLY | O_DIRECTORY, 0);
  if (fd < 0) {
    return false;
  }
  close(fd);
  return true;
}

// Resolves the parent once and protects the guest root from mutation
static int guest_parent(const char *path, char *name) {
  char relative[GUEST_PATH_MAX];
  if (!normalize_guest_path(path, relative)) {
    return -1;
  }
  if (*relative == '\0') {
    errno = EBUSY;
    return -1;
  }
  char *slash = strrchr(relative, '/');
  strcpy(name, slash == NULL ? relative : slash + 1);
  if (slash == NULL) {
    return dup(root_fd);
  }
  *slash = '\0';
  return open_beneath(root_fd, relative, O_RDONLY | O_DIRECTORY, 0);
}

int guest_mkdir(const char *path) {
  char name[GUEST_PATH_MAX];
  int parent = guest_parent(path, name);
  if (parent < 0) {
    return -1;
  }
  int result = mkdirat(parent, name, 0777);
  close(parent);
  return result;
}

static int guest_remove(const char *path, int flags) {
  char name[GUEST_PATH_MAX];
  int parent = guest_parent(path, name);
  if (parent < 0) {
    return -1;
  }
  int result = unlinkat(parent, name, flags);
  close(parent);
  return result;
}

int guest_unlink(const char *path) { return guest_remove(path, 0); }
int guest_rmdir(const char *path) { return guest_remove(path, AT_REMOVEDIR); }

int guest_move(const char *old_path, const char *new_path) {
  char old_name[GUEST_PATH_MAX], new_name[GUEST_PATH_MAX];
  int old_parent = guest_parent(old_path, old_name);
  if (old_parent < 0) {
    return -1;
  }
  int new_parent = guest_parent(new_path, new_name);
  int result = new_parent < 0
                   ? -1
                   : renameat(old_parent, old_name, new_parent, new_name);
  close(old_parent);
  if (new_parent >= 0) {
    close(new_parent);
  }
  return result;
}

int guest_touch(const char *path) {
  FILE *file = guest_fopen(path, "ab");
  if (file == NULL) {
    return -1;
  }
  int result = futimens(fileno(file), NULL);
  fclose(file);
  return result;
}

static bool read_guest_directory(const char *path, DirectoryList *list) {
  int fd = open_guest_path(path, O_RDONLY | O_DIRECTORY, 0);
  if (fd < 0) {
    return false;
  }
  DIR *directory = fdopendir(fd);
  if (directory == NULL) {
    close(fd);
    return false;
  }
  bool success = true;
  struct dirent *entry;
  errno = 0;
  while ((entry = readdir(directory)) != NULL) {
    if (!add_directory_entry(list, entry->d_name, entry->d_type == DT_DIR)) {
      success = false;
      break;
    }
    errno = 0;
  }
  if (errno != 0) {
    success = false;
  }
  closedir(directory);
  return success;
}
#endif

static int compare_directory_entries(const void *left, const void *right) {
  return strcmp(*(const char *const *)left + 2,
                *(const char *const *)right + 2);
}

char *guest_list_directory(const char *path) {
  DirectoryList list = {0};
  bool success = read_guest_directory(path, &list);
  if (list.count > 1) {
    qsort(list.entries, list.count, sizeof(*list.entries),
          compare_directory_entries);
  }
  size_t size = 1;
  for (size_t i = 0; i < list.count; i++) {
    size += strlen(list.entries[i]);
  }
  char *output = success ? malloc(size) : NULL;
  size_t offset = 0;
  if (output != NULL) {
    output[0] = '\0';
  }
  for (size_t i = 0; i < list.count; i++) {
    if (output != NULL) {
      strcpy(output + offset, list.entries[i]);
      offset += strlen(list.entries[i]);
    }
    free(list.entries[i]);
  }
  free(list.entries);
  return output;
}
