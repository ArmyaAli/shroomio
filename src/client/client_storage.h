#ifndef SHROOM_CLIENT_STORAGE_H
#define SHROOM_CLIENT_STORAGE_H

#include <stdbool.h>
#include <stddef.h>

#define SHROOM_CLIENT_STORAGE_PATH_MAX 512u

typedef enum ShroomClientStorageLocation {
  SHROOM_CLIENT_STORAGE_CACHE = 0,
  SHROOM_CLIENT_STORAGE_CONFIG,
} ShroomClientStorageLocation;

bool ShroomClientStorageDefaultFile(char* destination, size_t destination_size,
                                    ShroomClientStorageLocation location, const char* filename);
bool ShroomClientStorageEnsurePrivateParent(const char* file_path);
bool ShroomClientStorageCreatePrivateTemporaryFile(const char* destination_path,
                                                   char* temporary_path, size_t temporary_path_size,
                                                   int* descriptor);
bool ShroomClientStorageReplaceFile(const char* temporary_path, const char* destination_path);

#ifdef TEST_MODE
void ShroomClientStorageSetTestConfigRoot(const char* config_root);
#endif

#endif
