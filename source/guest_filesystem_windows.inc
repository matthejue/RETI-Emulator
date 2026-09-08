// Uses handle-relative Windows operations so junctions cannot bypass the root
#include <io.h>
#include <stddef.h>
#include <windows.h>
#include <winternl.h>

typedef NTSTATUS(NTAPI *CreateRelativeFile)(PHANDLE, ACCESS_MASK,
                                            POBJECT_ATTRIBUTES,
                                            PIO_STATUS_BLOCK, PLARGE_INTEGER,
                                            ULONG, ULONG, ULONG, ULONG, PVOID,
                                            ULONG);
typedef NTSTATUS(NTAPI *SetRelativeFileInformation)(HANDLE, PIO_STATUS_BLOCK,
                                                    PVOID, ULONG,
                                                    FILE_INFORMATION_CLASS);

static HANDLE root_handle = INVALID_HANDLE_VALUE;
static CreateRelativeFile create_relative_file;
static SetRelativeFileInformation set_relative_file_information;

void close_guest_filesystem(void) {
  if (root_handle != INVALID_HANDLE_VALUE) {
    CloseHandle(root_handle);
  }
  root_handle = INVALID_HANDLE_VALUE;
}

bool init_guest_filesystem(void) {
  close_guest_filesystem();
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  create_relative_file =
      (CreateRelativeFile)GetProcAddress(ntdll, "NtCreateFile");
  set_relative_file_information =
      (SetRelativeFileInformation)GetProcAddress(ntdll, "NtSetInformationFile");
  if (create_relative_file == NULL || set_relative_file_information == NULL) {
    return false;
  }
  root_handle = CreateFileW(L".", FILE_LIST_DIRECTORY | SYNCHRONIZE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  return root_handle != INVALID_HANDLE_VALUE;
}

static HANDLE open_relative(HANDLE root, const char *path, ACCESS_MASK access,
                            ULONG disposition, ULONG kind) {
  WCHAR name[GUEST_PATH_MAX];
  int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, name,
                                  GUEST_PATH_MAX);
  if (count == 0) {
    errno = EINVAL;
    return INVALID_HANDLE_VALUE;
  }
  for (int i = 0; i < count - 1; i++) {
    if (name[i] == L'/') {
      name[i] = L'\\';
    }
  }
  UNICODE_STRING unicode = {
      .Length = (USHORT)((count - 1) * sizeof(WCHAR)),
      .MaximumLength = (USHORT)(count * sizeof(WCHAR)),
      .Buffer = name,
  };
  OBJECT_ATTRIBUTES attributes = {
      .Length = sizeof(attributes),
      .RootDirectory = root,
      .ObjectName = &unicode,
      // OBJ_DONT_REPARSE rejects symlinks and every other reparse point
      .Attributes = OBJ_CASE_INSENSITIVE | OBJ_DONT_REPARSE,
  };
  IO_STATUS_BLOCK status;
  HANDLE handle;
  NTSTATUS result = create_relative_file(
      &handle, access | SYNCHRONIZE, &attributes, &status, NULL,
      FILE_ATTRIBUTE_NORMAL,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, disposition,
      kind | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_REPARSE_POINT, NULL, 0);
  if (result < 0) {
    errno = result == (NTSTATUS)0xC0000034 || result == (NTSTATUS)0xC000003A
                ? ENOENT
                : EACCES;
    return INVALID_HANDLE_VALUE;
  }
  BY_HANDLE_FILE_INFORMATION info;
  if (!GetFileInformationByHandle(handle, &info) ||
      (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
    CloseHandle(handle);
    errno = EACCES;
    return INVALID_HANDLE_VALUE;
  }
  return handle;
}

static HANDLE open_guest_path(const char *path, ACCESS_MASK access,
                              ULONG disposition, ULONG kind) {
  char relative[GUEST_PATH_MAX];
  if (!normalize_guest_path(path, relative)) {
    return INVALID_HANDLE_VALUE;
  }
  return open_relative(root_handle, relative, access, disposition, kind);
}

FILE *guest_fopen(const char *path, const char *mode) {
  bool read_only = strcmp(mode, "rb") == 0;
  bool update = strchr(mode, '+') != NULL;
  HANDLE handle =
      open_guest_path(path,
                      update      ? GENERIC_READ | GENERIC_WRITE
                      : read_only ? GENERIC_READ
                                  : GENERIC_WRITE,
                      *mode == 'w' || *mode == 'a' ? FILE_OPEN_IF : FILE_OPEN,
                      FILE_NON_DIRECTORY_FILE);
  if (handle == INVALID_HANDLE_VALUE) {
    return NULL;
  }
  BY_HANDLE_FILE_INFORMATION info;
  if (!GetFileInformationByHandle(handle, &info) || info.nNumberOfLinks != 1 ||
      GetFileType(handle) != FILE_TYPE_DISK) {
    CloseHandle(handle);
    errno = EACCES;
    return NULL;
  }
  if (*mode == 'w' && !SetEndOfFile(handle)) {
    CloseHandle(handle);
    return NULL;
  }
  int flags = _O_BINARY | (update      ? _O_RDWR
                           : read_only ? _O_RDONLY
                                       : _O_WRONLY);
  if (*mode == 'a') {
    flags |= _O_APPEND;
  }
  int fd = _open_osfhandle((intptr_t)handle, flags);
  if (fd < 0) {
    CloseHandle(handle);
    return NULL;
  }
  FILE *file = _fdopen(fd, mode);
  if (file == NULL) {
    _close(fd);
  }
  return file;
}

bool guest_is_directory(const char *path) {
  HANDLE handle = open_guest_path(path, FILE_LIST_DIRECTORY, FILE_OPEN,
                                  FILE_DIRECTORY_FILE);
  if (handle == INVALID_HANDLE_VALUE) {
    return false;
  }
  CloseHandle(handle);
  return true;
}

static HANDLE guest_parent(const char *path, char *name) {
  char relative[GUEST_PATH_MAX];
  if (!normalize_guest_path(path, relative)) {
    return INVALID_HANDLE_VALUE;
  }
  if (*relative == '\0') {
    errno = EBUSY;
    return INVALID_HANDLE_VALUE;
  }
  char *slash = strrchr(relative, '/');
  strcpy(name, slash == NULL ? relative : slash + 1);
  if (slash == NULL) {
    relative[0] = '\0';
  } else {
    *slash = '\0';
  }
  return open_relative(root_handle, relative, FILE_LIST_DIRECTORY, FILE_OPEN,
                       FILE_DIRECTORY_FILE);
}

int guest_mkdir(const char *path) {
  char name[GUEST_PATH_MAX];
  HANDLE parent = guest_parent(path, name);
  if (parent == INVALID_HANDLE_VALUE) {
    return -1;
  }
  HANDLE handle = open_relative(parent, name, FILE_LIST_DIRECTORY, FILE_CREATE,
                                FILE_DIRECTORY_FILE);
  CloseHandle(parent);
  if (handle == INVALID_HANDLE_VALUE) {
    return -1;
  }
  CloseHandle(handle);
  return 0;
}

static int guest_remove(const char *path, ULONG kind) {
  char name[GUEST_PATH_MAX];
  HANDLE parent = guest_parent(path, name);
  if (parent == INVALID_HANDLE_VALUE) {
    return -1;
  }
  HANDLE handle = open_relative(parent, name, DELETE, FILE_OPEN, kind);
  CloseHandle(parent);
  if (handle == INVALID_HANDLE_VALUE) {
    return -1;
  }
  BOOLEAN remove = TRUE;
  IO_STATUS_BLOCK status;
  NTSTATUS result = set_relative_file_information(
      handle, &status, &remove, sizeof(remove), FileDispositionInformation);
  CloseHandle(handle);
  return result < 0 ? -1 : 0;
}

int guest_unlink(const char *path) {
  return guest_remove(path, FILE_NON_DIRECTORY_FILE);
}
int guest_rmdir(const char *path) {
  return guest_remove(path, FILE_DIRECTORY_FILE);
}

int guest_move(const char *old_path, const char *new_path) {
  char old_name[GUEST_PATH_MAX], new_name[GUEST_PATH_MAX];
  HANDLE old_parent = guest_parent(old_path, old_name);
  if (old_parent == INVALID_HANDLE_VALUE) {
    return -1;
  }
  HANDLE handle = open_relative(old_parent, old_name, DELETE, FILE_OPEN, 0);
  CloseHandle(old_parent);
  if (handle == INVALID_HANDLE_VALUE) {
    return -1;
  }
  HANDLE new_parent = guest_parent(new_path, new_name);
  int result = -1;
  if (new_parent != INVALID_HANDLE_VALUE) {
    struct {
      BOOLEAN replace;
      HANDLE root;
      ULONG length;
      WCHAR name[GUEST_PATH_MAX];
    } rename_info = {.replace = TRUE, .root = new_parent};
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, new_name, -1,
                                    rename_info.name, GUEST_PATH_MAX);
    if (count > 0) {
      rename_info.length = (count - 1) * sizeof(WCHAR);
      IO_STATUS_BLOCK status;
      NTSTATUS status_code = set_relative_file_information(
          handle, &status, &rename_info,
          offsetof(typeof(rename_info), name) + rename_info.length,
          FileRenameInformation);
      result = status_code < 0 ? -1 : 0;
    }
    CloseHandle(new_parent);
  }
  CloseHandle(handle);
  return result;
}

int guest_touch(const char *path) {
  FILE *file = guest_fopen(path, "ab");
  if (file == NULL) {
    return -1;
  }
  FILETIME now;
  GetSystemTimeAsFileTime(&now);
  int result =
      SetFileTime((HANDLE)_get_osfhandle(_fileno(file)), NULL, &now, &now) ? 0
                                                                           : -1;
  fclose(file);
  return result;
}

static bool read_guest_directory(const char *path, DirectoryList *list) {
  HANDLE handle = open_guest_path(path, FILE_LIST_DIRECTORY, FILE_OPEN,
                                  FILE_DIRECTORY_FILE);
  if (handle == INVALID_HANDLE_VALUE) {
    return false;
  }
  union {
    FILE_ID_BOTH_DIR_INFO entry;
    char bytes[16384];
  } buffer;
  bool success = true;
  FILE_INFO_BY_HANDLE_CLASS information = FileIdBothDirectoryRestartInfo;
  while (success) {
    if (!GetFileInformationByHandleEx(handle, information, &buffer,
                                      sizeof(buffer))) {
      success = GetLastError() == ERROR_NO_MORE_FILES;
      break;
    }
    information = FileIdBothDirectoryInfo;
    FILE_ID_BOTH_DIR_INFO *entry = &buffer.entry;
    while (true) {
      char name[GUEST_PATH_MAX];
      int count = WideCharToMultiByte(CP_UTF8, 0, entry->FileName,
                                      entry->FileNameLength / sizeof(WCHAR),
                                      name, sizeof(name) - 1, NULL, NULL);
      if (count == 0) {
        success = false;
        break;
      }
      name[count] = '\0';
      if (!add_directory_entry(
              list, name,
              (entry->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)) {
        success = false;
        break;
      }
      if (entry->NextEntryOffset == 0) {
        break;
      }
      entry = (FILE_ID_BOTH_DIR_INFO *)((char *)entry + entry->NextEntryOffset);
    }
  }
  CloseHandle(handle);
  return success;
}
