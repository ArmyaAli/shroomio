#include "server_browser_model.h"

#include <stddef.h>

void ShroomServerBrowserModelInit(ShroomServerBrowserModel* model) {
  if (model == NULL) {
    return;
  }
  model->discovery_state = SHROOM_SERVER_DISCOVERY_EMPTY;
  model->sort_key = SHROOM_SERVER_SORT_PING;
  model->sort_descending = false;
}

void ShroomServerBrowserBeginRefresh(ShroomServerBrowserModel* model) {
  if (model != NULL) {
    model->discovery_state = SHROOM_SERVER_DISCOVERY_LOADING;
  }
}

void ShroomServerBrowserFinishRefresh(ShroomServerBrowserModel* model, bool succeeded,
                                      size_t result_count) {
  if (model == NULL) {
    return;
  }
  if (!succeeded) {
    model->discovery_state = SHROOM_SERVER_DISCOVERY_FAILED;
  } else if (result_count == 0u) {
    model->discovery_state = SHROOM_SERVER_DISCOVERY_EMPTY;
  } else {
    model->discovery_state = SHROOM_SERVER_DISCOVERY_READY;
  }
}

void ShroomServerBrowserMarkStale(ShroomServerBrowserModel* model) {
  if ((model != NULL) && (model->discovery_state == SHROOM_SERVER_DISCOVERY_READY)) {
    model->discovery_state = SHROOM_SERVER_DISCOVERY_STALE;
  }
}

void ShroomServerBrowserCancelRefresh(ShroomServerBrowserModel* model) {
  if ((model != NULL) && (model->discovery_state == SHROOM_SERVER_DISCOVERY_LOADING)) {
    model->discovery_state = SHROOM_SERVER_DISCOVERY_CANCELLED;
  }
}

void ShroomServerBrowserSetSort(ShroomServerBrowserModel* model, ShroomServerSortKey key) {
  if ((model == NULL) || (key < SHROOM_SERVER_SORT_NAME) || (key > SHROOM_SERVER_SORT_PING)) {
    return;
  }
  if (model->sort_key == key) {
    model->sort_descending = !model->sort_descending;
  } else {
    model->sort_key = key;
    model->sort_descending = false;
  }
}

const char* ShroomServerBrowserSortLabel(ShroomServerSortKey key) {
  switch (key) {
  case SHROOM_SERVER_SORT_PLAYERS:
    return "Players";
  case SHROOM_SERVER_SORT_PING:
    return "Ping";
  case SHROOM_SERVER_SORT_NAME:
  default:
    return "Name";
  }
}
