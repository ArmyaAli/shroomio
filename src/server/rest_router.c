#include "rest_router.h"

#include <string.h>

ShroomRestRoute ShroomRestClassifyRoute(const char* method, const char* path) {
  if ((method == NULL) || (path == NULL)) {
    return SHROOM_REST_ROUTE_NOT_FOUND;
  }
  if ((strcmp(method, "GET") == 0) && (strcmp(path, "/health") == 0)) {
    return SHROOM_REST_ROUTE_HEALTH;
  }
  return SHROOM_REST_ROUTE_NOT_FOUND;
}
