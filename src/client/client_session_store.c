#include "client_session_store.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#include <windows.h>
#define SHROOM_CLOSE _close
#define SHROOM_COMMIT _commit
#define SHROOM_MKDIR(path) _mkdir(path)
#define SHROOM_OPEN _open
#define SHROOM_UNLINK _unlink
#define SHROOM_WRITE _write
#else
#include <unistd.h>
#define SHROOM_CLOSE close
#define SHROOM_COMMIT fsync
#define SHROOM_MKDIR(path) mkdir(path, 0700)
#define SHROOM_OPEN open
#define SHROOM_UNLINK unlink
#define SHROOM_WRITE write
#endif

#define SESSION_FILE_VERSION 1u

static void SecureZero(void* data, size_t size) {
  volatile unsigned char* cursor = data;

  while (size-- > 0u) {
    *cursor++ = 0u;
  }
}

static bool IsSafeField(const char* value, size_t capacity) {
  size_t length;

  if (value == NULL) {
    return false;
  }
  length = strlen(value);
  if ((length == 0u) || (length >= capacity)) {
    return false;
  }
  for (size_t index = 0u; index < length; ++index) {
    const unsigned char character = (unsigned char)value[index];
    if ((character < 33u) || (character > 126u) || (character == '=') || (character == '\\')) {
      return false;
    }
  }
  return true;
}

static bool MakeParentDirectories(const char* file_path) {
  char path[SHROOM_CLIENT_SESSION_PATH_MAX];
  size_t length;

  if ((file_path == NULL) || (strlen(file_path) >= sizeof(path))) {
    return false;
  }
  snprintf(path, sizeof(path), "%s", file_path);
  length = strlen(path);
  for (size_t index = 1u; index < length; ++index) {
    if ((path[index] != '/') && (path[index] != '\\')) {
      continue;
    }
#if defined(_WIN32)
    if ((index == 2u) && (path[1] == ':')) {
      continue;
    }
#endif
    path[index] = '\0';
    if ((SHROOM_MKDIR(path) != 0) && (errno != EEXIST)) {
      return false;
    }
    path[index] = file_path[index];
  }
  return true;
}

bool ShroomClientSessionDefaultPath(char* path, size_t path_size) {
  const char* root;
  int count;

  if ((path == NULL) || (path_size == 0u)) {
    return false;
  }
#if defined(_WIN32)
  root = getenv("APPDATA");
  if ((root == NULL) || (root[0] == '\0')) {
    return false;
  }
  count = snprintf(path, path_size, "%s\\shroomio\\session.cfg", root);
#elif defined(__APPLE__)
  root = getenv("HOME");
  if ((root == NULL) || (root[0] == '\0')) {
    return false;
  }
  count = snprintf(path, path_size, "%s/Library/Application Support/shroomio/session.cfg", root);
#else
  root = getenv("XDG_CONFIG_HOME");
  if ((root != NULL) && (root[0] != '\0')) {
    count = snprintf(path, path_size, "%s/shroomio/session.cfg", root);
  } else {
    root = getenv("HOME");
    if ((root == NULL) || (root[0] == '\0')) {
      return false;
    }
    count = snprintf(path, path_size, "%s/.config/shroomio/session.cfg", root);
  }
#endif
  return (count > 0) && ((size_t)count < path_size);
}

static bool WriteAll(int descriptor, const char* data, size_t length) {
  size_t written = 0u;

  while (written < length) {
    const int count = (int)SHROOM_WRITE(descriptor, data + written, length - written);
    if (count <= 0) {
      return false;
    }
    written += (size_t)count;
  }
  return true;
}

bool ShroomClientSessionSave(const char* path, const ShroomClientStoredSession* session) {
  char temporary_path[SHROOM_CLIENT_SESSION_PATH_MAX + 8u];
  char contents[768] = {0};
  int descriptor;
  int length;
  bool success;

  if ((path == NULL) || (session == NULL) || (strlen(path) >= SHROOM_CLIENT_SESSION_PATH_MAX) ||
      !IsSafeField(session->base_url, sizeof(session->base_url)) ||
      !IsSafeField(session->refresh_token, sizeof(session->refresh_token)) ||
      (session->refresh_expires_at == 0u) || !MakeParentDirectories(path)) {
    return false;
  }
  length = snprintf(contents, sizeof(contents),
                    "version=%u\nbase_url=%s\nrefresh_token=%s\nrefresh_expires_at=%llu\n",
                    SESSION_FILE_VERSION, session->base_url, session->refresh_token,
                    (unsigned long long)session->refresh_expires_at);
  if ((length <= 0) || ((size_t)length >= sizeof(contents))) {
    SecureZero(contents, sizeof(contents));
    return false;
  }
  snprintf(temporary_path, sizeof(temporary_path), "%s.tmp", path);
  (void)SHROOM_UNLINK(temporary_path);
#if defined(_WIN32)
  descriptor =
      SHROOM_OPEN(temporary_path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
  descriptor = SHROOM_OPEN(temporary_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
#endif
  if (descriptor < 0) {
    SecureZero(contents, sizeof(contents));
    return false;
  }
  success = WriteAll(descriptor, contents, (size_t)length) && (SHROOM_COMMIT(descriptor) == 0);
  success = (SHROOM_CLOSE(descriptor) == 0) && success;
  SecureZero(contents, sizeof(contents));
  if (!success) {
    SHROOM_UNLINK(temporary_path);
    return false;
  }
#if defined(_WIN32)
  if (!MoveFileExA(temporary_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    SHROOM_UNLINK(temporary_path);
    return false;
  }
  if (_chmod(path, _S_IREAD | _S_IWRITE) != 0) {
    SHROOM_UNLINK(path);
    return false;
  }
#else
  if ((chmod(temporary_path, 0600) != 0) || (rename(temporary_path, path) != 0)) {
    SHROOM_UNLINK(temporary_path);
    return false;
  }
  if (chmod(path, 0600) != 0) {
    SHROOM_UNLINK(path);
    return false;
  }
#endif
  return true;
}

bool ShroomClientSessionLoad(const char* path, ShroomClientStoredSession* session) {
  FILE* file;
  char line[384];
  unsigned version = 0u;
  bool have_base_url = false;
  bool have_refresh = false;
  bool have_expiry = false;
  bool have_version = false;
  bool valid = true;
  unsigned long long expiry = 0u;
  ShroomClientStoredSession loaded = {0};

  if ((path == NULL) || (session == NULL)) {
    return false;
  }
  file = fopen(path, "rb");
  if (file == NULL) {
    return false;
  }
#if !defined(_WIN32)
  {
    struct stat metadata;
    if ((fstat(fileno(file), &metadata) != 0) || ((metadata.st_mode & 077u) != 0u)) {
      fclose(file);
      return false;
    }
  }
#endif
  while (fgets(line, sizeof(line), file) != NULL) {
    char* newline = strchr(line, '\n');
    if (newline == NULL) {
      fclose(file);
      SecureZero(&loaded, sizeof(loaded));
      return false;
    }
    *newline = '\0';
    char trailing;
    if (sscanf(line, "version=%u%c", &version, &trailing) == 1) {
      if (have_version) {
        valid = false;
        break;
      }
      have_version = true;
      continue;
    }
    if (strncmp(line, "base_url=", 9u) == 0) {
      const size_t length = strlen(line + 9u);
      if (have_base_url) {
        valid = false;
        break;
      }
      if (length >= sizeof(loaded.base_url)) {
        valid = false;
        break;
      }
      memcpy(loaded.base_url, line + 9u, length + 1u);
      have_base_url = true;
    } else if (strncmp(line, "refresh_token=", 14u) == 0) {
      const size_t length = strlen(line + 14u);
      if (have_refresh) {
        valid = false;
        break;
      }
      if (length >= sizeof(loaded.refresh_token)) {
        valid = false;
        break;
      }
      memcpy(loaded.refresh_token, line + 14u, length + 1u);
      have_refresh = true;
    } else if (sscanf(line, "refresh_expires_at=%llu%c", &expiry, &trailing) == 1) {
      if (have_expiry) {
        valid = false;
        break;
      }
      loaded.refresh_expires_at = (uint64_t)expiry;
      have_expiry = true;
    } else {
      valid = false;
      break;
    }
  }
  const bool read_failed = ferror(file) != 0;
  const bool close_failed = fclose(file) != 0;
  if (read_failed || close_failed || !valid || !have_version || (version != SESSION_FILE_VERSION) ||
      !have_base_url || !have_refresh || !have_expiry ||
      !IsSafeField(loaded.base_url, sizeof(loaded.base_url)) ||
      !IsSafeField(loaded.refresh_token, sizeof(loaded.refresh_token))) {
    SecureZero(&loaded, sizeof(loaded));
    return false;
  }
  *session = loaded;
  SecureZero(&loaded, sizeof(loaded));
  return true;
}

bool ShroomClientSessionDelete(const char* path) {
  if (path == NULL) {
    return false;
  }
  return (SHROOM_UNLINK(path) == 0) || (errno == ENOENT);
}
