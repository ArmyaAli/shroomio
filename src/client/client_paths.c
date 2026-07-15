#include "client_paths.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef TEST_MODE
static char g_test_cache_root[SHROOM_CLIENT_PATH_MAX];
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

static bool FormatPath(char* destination, size_t destination_size, const char* root,
                       const char* middle, const char* filename, char separator) {
  int written;

  if ((destination == NULL) || (destination_size == 0u) || (root == NULL) || (root[0] == '\0') ||
      !FilenameIsSafe(filename)) {
    return false;
  }
  written = middle != NULL
                ? snprintf(destination, destination_size, "%s%c%s%c%s", root, separator, middle,
                           separator, filename)
                : snprintf(destination, destination_size, "%s%c%s", root, separator, filename);
  return (written >= 0) && ((size_t)written < destination_size);
}

bool ShroomClientPathsBuildCacheFile(char* destination, size_t destination_size,
                                     ShroomClientPlatform platform, const char* home_directory,
                                     const char* platform_cache_directory, const char* filename) {
  char fallback[SHROOM_CLIENT_PATH_MAX];
  int written;

  switch (platform) {
  case SHROOM_CLIENT_PLATFORM_LINUX:
    if ((platform_cache_directory != NULL) && (platform_cache_directory[0] != '\0')) {
      return FormatPath(destination, destination_size, platform_cache_directory,
                        SHROOM_CLIENT_CACHE_DIRECTORY, filename, '/');
    }
    if ((home_directory == NULL) || (home_directory[0] == '\0')) {
      return false;
    }
    written = snprintf(fallback, sizeof(fallback), "%s/.cache", home_directory);
    return (written >= 0) && ((size_t)written < sizeof(fallback)) &&
           FormatPath(destination, destination_size, fallback, SHROOM_CLIENT_CACHE_DIRECTORY,
                      filename, '/');
  case SHROOM_CLIENT_PLATFORM_WINDOWS:
    if ((platform_cache_directory != NULL) && (platform_cache_directory[0] != '\0')) {
      return FormatPath(destination, destination_size, platform_cache_directory,
                        SHROOM_CLIENT_CACHE_DIRECTORY, filename, '\\');
    }
    if ((home_directory == NULL) || (home_directory[0] == '\0')) {
      return false;
    }
    written = snprintf(fallback, sizeof(fallback), "%s\\AppData\\Local", home_directory);
    return (written >= 0) && ((size_t)written < sizeof(fallback)) &&
           FormatPath(destination, destination_size, fallback, SHROOM_CLIENT_CACHE_DIRECTORY,
                      filename, '\\');
  case SHROOM_CLIENT_PLATFORM_MACOS:
    if ((home_directory == NULL) || (home_directory[0] == '\0')) {
      return false;
    }
    written = snprintf(fallback, sizeof(fallback), "%s/Library/Caches", home_directory);
    return (written >= 0) && ((size_t)written < sizeof(fallback)) &&
           FormatPath(destination, destination_size, fallback, SHROOM_CLIENT_CACHE_DIRECTORY,
                      filename, '/');
  default:
    return false;
  }
}

bool ShroomClientPathsGetCacheFile(char* destination, size_t destination_size,
                                   const char* filename) {
#ifdef TEST_MODE
  if (g_test_cache_root[0] != '\0') {
#ifdef _WIN32
    return FormatPath(destination, destination_size, g_test_cache_root,
                      SHROOM_CLIENT_CACHE_DIRECTORY, filename, '\\');
#else
    return FormatPath(destination, destination_size, g_test_cache_root,
                      SHROOM_CLIENT_CACHE_DIRECTORY, filename, '/');
#endif
  }
#endif
  return ShroomClientStorageDefaultFile(destination, destination_size, SHROOM_CLIENT_STORAGE_CACHE,
                                        filename);
}

static bool FileIsRegular(const char* path, size_t maximum_bytes) {
  struct stat status;

  if (path == NULL) {
    return false;
  }
#ifdef _WIN32
  const DWORD attributes = GetFileAttributesA(path);
  if ((attributes == INVALID_FILE_ATTRIBUTES) ||
      ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u)) {
    return false;
  }
  return (stat(path, &status) == 0) && S_ISREG(status.st_mode) && (status.st_size >= 0) &&
         ((uintmax_t)status.st_size <= maximum_bytes);
#else
  return (lstat(path, &status) == 0) && S_ISREG(status.st_mode) && (status.st_size >= 0) &&
         ((uintmax_t)status.st_size <= maximum_bytes);
#endif
}

static FILE* OpenRegularFileForRead(const char* path, size_t maximum_bytes) {
  struct stat status;
  int descriptor;

#ifdef _WIN32
  descriptor = _open(path, _O_RDONLY | _O_BINARY | _O_NOINHERIT);
#else
  int flags = O_RDONLY;
#ifdef O_CLOEXEC
  flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
  flags |= O_NOFOLLOW;
#endif
  descriptor = open(path, flags);
#endif
  if (descriptor < 0) {
    return NULL;
  }
#ifdef _WIN32
  FILE* file;

  if ((fstat(descriptor, &status) != 0) || !S_ISREG(status.st_mode) || (status.st_size < 0) ||
      ((uintmax_t)status.st_size > maximum_bytes)) {
    _close(descriptor);
    return NULL;
  }
  file = _fdopen(descriptor, "rb");
  if (file == NULL) {
    _close(descriptor);
  }
  return file;
#else
  FILE* file;

  if ((fstat(descriptor, &status) != 0) || !S_ISREG(status.st_mode) || (status.st_size < 0) ||
      ((uintmax_t)status.st_size > maximum_bytes)) {
    close(descriptor);
    return NULL;
  }
  file = fdopen(descriptor, "rb");
  if (file == NULL) {
    close(descriptor);
  }
  return file;
#endif
}

static bool CopyLegacyFile(const char* source_path, const char* destination_path,
                           size_t maximum_bytes, ShroomClientPathValidator validator,
                           const void* validator_context) {
  char temporary_path[SHROOM_CLIENT_PATH_MAX + 64u] = {0};
  unsigned char buffer[4096];
  FILE* source = NULL;
  FILE* destination = NULL;
  int descriptor = -1;
  size_t total = 0u;
  bool success = false;

  if (!FileIsRegular(source_path, maximum_bytes)) {
    return false;
  }
  source = OpenRegularFileForRead(source_path, maximum_bytes);
  if (source == NULL) {
    return false;
  }
  if (!ShroomClientStorageCreatePrivateTemporaryFile(destination_path, temporary_path,
                                                     sizeof(temporary_path), &descriptor)) {
    goto cleanup;
  }
  destination = fdopen(descriptor, "wb");
  if (destination == NULL) {
#ifdef _WIN32
    _close(descriptor);
#else
    close(descriptor);
#endif
    descriptor = -1;
    goto cleanup;
  }
  while (!feof(source)) {
    const size_t read_count = fread(buffer, 1u, sizeof(buffer), source);

    if ((read_count == 0u) && ferror(source)) {
      goto cleanup;
    }
    if ((read_count > maximum_bytes - total) ||
        (read_count > 0u && fwrite(buffer, 1u, read_count, destination) != read_count)) {
      goto cleanup;
    }
    total += read_count;
  }
  if (fflush(destination) != 0) {
    goto cleanup;
  }
#ifdef _WIN32
  if (_commit(descriptor) != 0) {
#else
  if (fsync(descriptor) != 0) {
#endif
    goto cleanup;
  }
  const int close_result = fclose(destination);
  destination = NULL;
  descriptor = -1;
  if (close_result != 0) {
    goto cleanup;
  }
  if ((validator != NULL) && !validator(temporary_path, validator_context)) {
    goto cleanup;
  }
  if (!ShroomClientStorageReplaceFile(temporary_path, destination_path)) {
    goto cleanup;
  }
  success = true;

cleanup:
  if (destination != NULL) {
    fclose(destination);
  } else if (descriptor >= 0) {
#ifdef _WIN32
    _close(descriptor);
#else
    close(descriptor);
#endif
  }
  if (source != NULL) {
    fclose(source);
  }
  if (!success) {
    remove(temporary_path);
  }
  return success;
}

static bool PrepareFailed(char* destination, size_t destination_size) {
  if ((destination != NULL) && (destination_size > 0u)) {
    destination[0] = '\0';
  }
  return false;
}

bool ShroomClientPathsPrepareCacheFile(char* destination, size_t destination_size,
                                       const char* filename, const char* legacy_path,
                                       size_t maximum_bytes, ShroomClientPathValidator validator,
                                       const void* validator_context) {
  struct stat destination_status;
  struct stat legacy_status;
  bool destination_exists;

  if ((maximum_bytes == 0u) ||
      !ShroomClientPathsGetCacheFile(destination, destination_size, filename) ||
      !ShroomClientStorageEnsurePrivateParent(destination)) {
    return PrepareFailed(destination, destination_size);
  }
  destination_exists = stat(destination, &destination_status) == 0;
  if (!destination_exists && (errno != ENOENT)) {
    return PrepareFailed(destination, destination_size);
  }
  if (FileIsRegular(destination, maximum_bytes) &&
      ((validator == NULL) || validator(destination, validator_context))) {
    if ((legacy_path != NULL) && (strcmp(legacy_path, destination) != 0)) {
      remove(legacy_path);
    }
    return true;
  }
  if ((legacy_path == NULL) || (stat(legacy_path, &legacy_status) != 0)) {
    if ((errno == ENOENT) && !destination_exists) {
      return true;
    }
    return PrepareFailed(destination, destination_size);
  }
  if (!FileIsRegular(legacy_path, maximum_bytes) ||
      ((validator != NULL) && !validator(legacy_path, validator_context)) ||
      !CopyLegacyFile(legacy_path, destination, maximum_bytes, validator, validator_context)) {
    return PrepareFailed(destination, destination_size);
  }
  remove(legacy_path);
  return true;
}

#ifdef TEST_MODE
void ShroomClientPathsSetTestCacheRoot(const char* cache_root) {
  snprintf(g_test_cache_root, sizeof(g_test_cache_root), "%s",
           cache_root != NULL ? cache_root : "");
}
#endif
