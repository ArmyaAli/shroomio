#include "client_storage.h"

#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#include <windows.h>
#define SHROOM_CSIDL_APPDATA 0x001a
#define SHROOM_CSIDL_LOCAL_APPDATA 0x001c
#define SHROOM_SHGFP_TYPE_CURRENT 0
HRESULT WINAPI SHGetFolderPathA(void* owner, int folder, HANDLE token, DWORD flags, char* path);
#else
#include <pwd.h>
#include <unistd.h>
#endif

#ifdef TEST_MODE
static char g_test_config_root[SHROOM_CLIENT_STORAGE_PATH_MAX];
#endif

static bool IsSeparator(char character) { return (character == '/') || (character == '\\'); }

static bool FilenameIsSafe(const char* filename) {
  if ((filename == NULL) || (filename[0] == '\0') || (strcmp(filename, ".") == 0) ||
      (strcmp(filename, "..") == 0)) {
    return false;
  }
  for (const char* cursor = filename; *cursor != '\0'; ++cursor) {
    if (IsSeparator(*cursor)) {
      return false;
    }
  }
  return true;
}

bool ShroomClientStorageDefaultFile(char* destination, size_t destination_size,
                                    ShroomClientStorageLocation location, const char* filename) {
  char root[SHROOM_CLIENT_STORAGE_PATH_MAX];
  int length;

  if ((destination == NULL) || (destination_size == 0u) || !FilenameIsSafe(filename)) {
    return false;
  }
#ifdef TEST_MODE
  if ((location == SHROOM_CLIENT_STORAGE_CONFIG) && (g_test_config_root[0] != '\0')) {
    length = snprintf(destination, destination_size, "%s/%s", g_test_config_root, filename);
    return (length > 0) && ((size_t)length < destination_size);
  }
#endif
#ifdef _WIN32
  const int folder =
      location == SHROOM_CLIENT_STORAGE_CACHE ? SHROOM_CSIDL_LOCAL_APPDATA : SHROOM_CSIDL_APPDATA;
  if (SHGetFolderPathA(NULL, folder, NULL, SHROOM_SHGFP_TYPE_CURRENT, root) != S_OK) {
    return false;
  }
  length = snprintf(destination, destination_size, "%s\\shroomio\\%s", root, filename);
#else
  const struct passwd* account = getpwuid(getuid());
  if ((account == NULL) || (account->pw_dir == NULL) || (account->pw_dir[0] == '\0')) {
    return false;
  }
#ifdef __APPLE__
  length = snprintf(root, sizeof(root), "%s/Library/%s", account->pw_dir,
                    location == SHROOM_CLIENT_STORAGE_CACHE ? "Caches" : "Application Support");
#else
  length = snprintf(root, sizeof(root), "%s/%s", account->pw_dir,
                    location == SHROOM_CLIENT_STORAGE_CACHE ? ".cache" : ".config");
#endif
  if ((length < 0) || ((size_t)length >= sizeof(root))) {
    return false;
  }
  length = snprintf(destination, destination_size, "%s/shroomio/%s", root, filename);
#endif
  return (length > 0) && ((size_t)length < destination_size);
}

#ifdef TEST_MODE
void ShroomClientStorageSetTestConfigRoot(const char* config_root) {
  snprintf(g_test_config_root, sizeof(g_test_config_root), "%s",
           config_root != NULL ? config_root : "");
}
#endif

static bool DirectoryIsUsable(const char* path, bool require_private) {
#ifdef _WIN32
  const DWORD attributes = GetFileAttributesA(path);

  (void)require_private;
  return (attributes != INVALID_FILE_ATTRIBUTES) &&
         ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) &&
         ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0u);
#else
  struct stat status;

  return (lstat(path, &status) == 0) && S_ISDIR(status.st_mode) &&
         (!require_private || ((status.st_mode & 0077) == 0));
#endif
}

static bool CreateDirectory(const char* path, bool require_private) {
  if (DirectoryIsUsable(path, require_private)) {
    return true;
  }
  errno = 0;
#ifdef _WIN32
  if (_mkdir(path) == 0) {
#else
  if (mkdir(path, 0700) == 0) {
#endif
    return DirectoryIsUsable(path, require_private);
  }
  return (errno == EEXIST) && DirectoryIsUsable(path, require_private);
}

bool ShroomClientStorageEnsurePrivateParent(const char* file_path) {
  char directory[SHROOM_CLIENT_STORAGE_PATH_MAX];
  char* last_separator = NULL;
  size_t start = 1u;
  const int length =
      file_path != NULL ? snprintf(directory, sizeof(directory), "%s", file_path) : -1;

  if ((length < 0) || ((size_t)length >= sizeof(directory))) {
    return false;
  }
  for (char* cursor = directory; *cursor != '\0'; ++cursor) {
    if (IsSeparator(*cursor)) {
      last_separator = cursor;
    }
  }
  if (last_separator == NULL) {
    return false;
  }
  *last_separator = '\0';
#ifdef _WIN32
  if ((directory[0] != '\0') && (directory[1] == ':')) {
    start = 3u;
  }
#endif
  for (size_t index = start; directory[index] != '\0'; ++index) {
    if (!IsSeparator(directory[index])) {
      continue;
    }
    const char separator = directory[index];
    directory[index] = '\0';
    if ((directory[0] != '\0') && !CreateDirectory(directory, false)) {
      return false;
    }
    directory[index] = separator;
  }
  return CreateDirectory(directory, true);
}

bool ShroomClientStorageCreatePrivateTemporaryFile(const char* destination_path,
                                                   char* temporary_path, size_t temporary_path_size,
                                                   int* descriptor) {
  static atomic_uint temporary_counter;

  if ((destination_path == NULL) || (temporary_path == NULL) || (temporary_path_size == 0u) ||
      (descriptor == NULL)) {
    return false;
  }
  *descriptor = -1;
  for (unsigned int attempt = 0u; attempt < 32u; ++attempt) {
#ifdef _WIN32
    const int process_id = _getpid();
#else
    const int process_id = (int)getpid();
#endif
    const unsigned int identifier = atomic_fetch_add(&temporary_counter, 1u) + 1u;
    const int length = snprintf(temporary_path, temporary_path_size, "%s.tmp.%d.%u",
                                destination_path, process_id, identifier);
    if ((length < 0) || ((size_t)length >= temporary_path_size)) {
      return false;
    }
#ifdef _WIN32
    *descriptor = _open(temporary_path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY | _O_NOINHERIT,
                        _S_IREAD | _S_IWRITE);
#else
    int flags = O_WRONLY | O_CREAT | O_EXCL;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    *descriptor = open(temporary_path, flags, 0600);
#endif
    if (*descriptor >= 0) {
      return true;
    }
    if (errno != EEXIST) {
      break;
    }
  }
  temporary_path[0] = '\0';
  return false;
}

bool ShroomClientStorageReplaceFile(const char* temporary_path, const char* destination_path) {
  if ((temporary_path == NULL) || (destination_path == NULL)) {
    return false;
  }
#ifdef _WIN32
  return MoveFileExA(temporary_path, destination_path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  return rename(temporary_path, destination_path) == 0;
#endif
}
