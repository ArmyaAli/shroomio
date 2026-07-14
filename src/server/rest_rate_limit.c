#include "rest_rate_limit.h"

#include <stdio.h>
#include <string.h>

#define RATE_WINDOW_SECONDS 60u

static uint32_t LimitForRoute(ShroomRestRateRoute route) {
  switch (route) {
  case SHROOM_REST_RATE_REGISTER:
  case SHROOM_REST_RATE_LOGIN:
    return 5u;
  case SHROOM_REST_RATE_REFRESH:
    return 10u;
  case SHROOM_REST_RATE_AUTHENTICATED:
  default:
    return 60u;
  }
}

void ShroomRestRateLimiterInit(ShroomRestRateLimiter* limiter) {
  if (limiter == NULL) {
    return;
  }
  memset(limiter, 0, sizeof(*limiter));
  atomic_flag_clear(&limiter->lock);
}

static ShroomRestRateLimitEntry* FindEntry(ShroomRestRateLimiter* limiter, const char* key,
                                           ShroomRestRateRoute route) {
  ShroomRestRateLimitEntry* replacement = NULL;

  for (size_t index = 0u; index < SHROOM_REST_RATE_LIMIT_KEYS; ++index) {
    ShroomRestRateLimitEntry* entry = &limiter->entries[index];
    if (entry->active && (entry->route == route) && (strcmp(entry->key, key) == 0)) {
      return entry;
    }
    if (!entry->active || (replacement == NULL) || (entry->last_seen < replacement->last_seen)) {
      replacement = entry;
    }
  }
  memset(replacement, 0, sizeof(*replacement));
  snprintf(replacement->key, sizeof(replacement->key), "%s", key);
  replacement->route = route;
  replacement->active = true;
  return replacement;
}

ShroomRestRateLimitResult ShroomRestRateLimitCheck(ShroomRestRateLimiter* limiter, const char* key,
                                                   ShroomRestRateRoute route,
                                                   uint64_t now_seconds) {
  ShroomRestRateLimitResult result = {.limit = LimitForRoute(route)};
  ShroomRestRateLimitEntry* entry;
  size_t retained = 0u;

  if ((limiter == NULL) || (key == NULL) || (key[0] == '\0')) {
    return result;
  }
  while (atomic_flag_test_and_set(&limiter->lock)) {
  }
  entry = FindEntry(limiter, key, route);
  for (size_t index = 0u; index < entry->attempt_count; ++index) {
    if ((entry->attempts[index] + RATE_WINDOW_SECONDS) > now_seconds) {
      entry->attempts[retained++] = entry->attempts[index];
    }
  }
  entry->attempt_count = retained;
  entry->last_seen = now_seconds;
  if (entry->attempt_count >= result.limit) {
    result.allowed = false;
    result.remaining = 0u;
    result.reset_at = entry->attempts[0] + RATE_WINDOW_SECONDS;
    result.retry_after =
        result.reset_at > now_seconds ? (uint32_t)(result.reset_at - now_seconds) : 1u;
  } else {
    entry->attempts[entry->attempt_count++] = now_seconds;
    result.allowed = true;
    result.remaining = result.limit - (uint32_t)entry->attempt_count;
    result.reset_at = entry->attempts[0] + RATE_WINDOW_SECONDS;
  }
  atomic_flag_clear(&limiter->lock);
  return result;
}
