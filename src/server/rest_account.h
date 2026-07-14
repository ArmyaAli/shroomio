#ifndef SHROOM_REST_ACCOUNT_H
#define SHROOM_REST_ACCOUNT_H

struct mg_connection;
struct ShroomAccountAuth;
struct ShroomRestRateLimiter;

#include "rest_router.h"

int ShroomRestHandleAccount(struct mg_connection* connection, ShroomRestRoute route,
                            const char* request_id, struct ShroomAccountAuth* auth,
                            struct ShroomRestRateLimiter* limiter);

#endif
