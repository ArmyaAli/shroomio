#ifndef SHROOM_SERVER_BROWSER_MODEL_H
#define SHROOM_SERVER_BROWSER_MODEL_H

#include <stdbool.h>
#include <stddef.h>

typedef enum ShroomServerBrowserDiscoveryState {
  SHROOM_SERVER_DISCOVERY_EMPTY = 0,
  SHROOM_SERVER_DISCOVERY_LOADING,
  SHROOM_SERVER_DISCOVERY_READY,
  SHROOM_SERVER_DISCOVERY_FAILED,
  SHROOM_SERVER_DISCOVERY_STALE,
  SHROOM_SERVER_DISCOVERY_CANCELLED,
} ShroomServerBrowserDiscoveryState;

typedef enum ShroomServerSortKey {
  SHROOM_SERVER_SORT_NAME = 0,
  SHROOM_SERVER_SORT_PLAYERS,
  SHROOM_SERVER_SORT_PING,
} ShroomServerSortKey;

typedef struct ShroomServerBrowserModel {
  ShroomServerBrowserDiscoveryState discovery_state;
  ShroomServerSortKey sort_key;
  bool sort_descending;
} ShroomServerBrowserModel;

void ShroomServerBrowserModelInit(ShroomServerBrowserModel* model);
void ShroomServerBrowserBeginRefresh(ShroomServerBrowserModel* model);
void ShroomServerBrowserFinishRefresh(ShroomServerBrowserModel* model, bool succeeded,
                                      size_t result_count);
void ShroomServerBrowserMarkStale(ShroomServerBrowserModel* model);
void ShroomServerBrowserCancelRefresh(ShroomServerBrowserModel* model);
void ShroomServerBrowserSetSort(ShroomServerBrowserModel* model, ShroomServerSortKey key);
const char* ShroomServerBrowserSortLabel(ShroomServerSortKey key);

#endif
