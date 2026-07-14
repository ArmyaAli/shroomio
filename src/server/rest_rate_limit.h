#ifndef SHROOM_REST_RATE_LIMIT_H
#define SHROOM_REST_RATE_LIMIT_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SHROOM_REST_RATE_LIMIT_KEYS 128u
#define SHROOM_REST_RATE_LIMIT_MAX_ATTEMPTS 60u

typedef enum ShroomRestRateRoute {
  SHROOM_REST_RATE_REGISTER = 0,
  SHROOM_REST_RATE_LOGIN,
  SHROOM_REST_RATE_REFRESH,
  SHROOM_REST_RATE_AUTHENTICATED,
} ShroomRestRateRoute;

typedef struct ShroomRestRateLimitResult {
  bool allowed;
  uint32_t limit;
  uint32_t remaining;
  uint64_t reset_at;
  uint32_t retry_after;
} ShroomRestRateLimitResult;

typedef struct ShroomRestRateLimitEntry {
  char key[64];
  ShroomRestRateRoute route;
  uint64_t attempts[SHROOM_REST_RATE_LIMIT_MAX_ATTEMPTS];
  size_t attempt_count;
  uint64_t last_seen;
  bool active;
} ShroomRestRateLimitEntry;

typedef struct ShroomRestRateLimiter {
  atomic_flag lock;
  ShroomRestRateLimitEntry entries[SHROOM_REST_RATE_LIMIT_KEYS];
} ShroomRestRateLimiter;

void ShroomRestRateLimiterInit(ShroomRestRateLimiter* limiter);
ShroomRestRateLimitResult ShroomRestRateLimitCheck(ShroomRestRateLimiter* limiter, const char* key,
                                                   ShroomRestRateRoute route, uint64_t now_seconds);

#endif
