#include "rest_router.h"

#include <string.h>

ShroomRestRoute ShroomRestClassifyRoute(const char* method, const char* path) {
  if ((method == NULL) || (path == NULL)) {
    return SHROOM_REST_ROUTE_NOT_FOUND;
  }
  if ((strcmp(method, "GET") == 0) && (strcmp(path, "/health") == 0)) {
    return SHROOM_REST_ROUTE_HEALTH;
  }
  if ((strcmp(method, "POST") == 0) && (strcmp(path, "/v1/account/register") == 0)) {
    return SHROOM_REST_ROUTE_ACCOUNT_REGISTER;
  }
  if ((strcmp(method, "POST") == 0) && (strcmp(path, "/v1/account/login") == 0)) {
    return SHROOM_REST_ROUTE_ACCOUNT_LOGIN;
  }
  if ((strcmp(method, "POST") == 0) && (strcmp(path, "/v1/account/refresh") == 0)) {
    return SHROOM_REST_ROUTE_ACCOUNT_REFRESH;
  }
  if ((strcmp(method, "POST") == 0) && (strcmp(path, "/v1/account/logout") == 0)) {
    return SHROOM_REST_ROUTE_ACCOUNT_LOGOUT;
  }
  if ((strcmp(method, "GET") == 0) && (strcmp(path, "/v1/account/me") == 0)) {
    return SHROOM_REST_ROUTE_ACCOUNT_ME;
  }
  return SHROOM_REST_ROUTE_NOT_FOUND;
}
