#ifndef SHROOM_CLIENT_PATHS_H
#define SHROOM_CLIENT_PATHS_H

#include <stdbool.h>
#include <stddef.h>

#include "client_storage.h"

#define SHROOM_CLIENT_PATH_MAX SHROOM_CLIENT_STORAGE_PATH_MAX
#define SHROOM_CLIENT_CACHE_DIRECTORY "shroomio"

typedef enum ShroomClientPlatform {
  SHROOM_CLIENT_PLATFORM_LINUX = 0,
  SHROOM_CLIENT_PLATFORM_WINDOWS,
  SHROOM_CLIENT_PLATFORM_MACOS,
} ShroomClientPlatform;

typedef bool (*ShroomClientPathValidator)(const char* path, const void* context);

bool ShroomClientPathsBuildCacheFile(char* destination, size_t destination_size,
                                     ShroomClientPlatform platform, const char* home_directory,
                                     const char* platform_cache_directory, const char* filename);
bool ShroomClientPathsGetCacheFile(char* destination, size_t destination_size,
                                   const char* filename);
bool ShroomClientPathsPrepareCacheFile(char* destination, size_t destination_size,
                                       const char* filename, const char* legacy_path,
                                       size_t maximum_bytes, ShroomClientPathValidator validator,
                                       const void* validator_context);

#ifdef TEST_MODE
void ShroomClientPathsSetTestCacheRoot(const char* cache_root);
#endif

#endif
