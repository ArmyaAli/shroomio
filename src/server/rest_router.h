#ifndef SHROOM_REST_ROUTER_H
#define SHROOM_REST_ROUTER_H

typedef enum ShroomRestRoute {
  SHROOM_REST_ROUTE_NOT_FOUND = 0,
  SHROOM_REST_ROUTE_HEALTH
} ShroomRestRoute;

ShroomRestRoute ShroomRestClassifyRoute(const char* method, const char* path);

#endif
