#include "unity.h"

#include "server/rest_router.h"

void setUp(void) {}
void tearDown(void) {}

static void test_health_route_requires_exact_get_and_path(void) {
  TEST_ASSERT_EQUAL_INT(SHROOM_REST_ROUTE_HEALTH, ShroomRestClassifyRoute("GET", "/health"));
  TEST_ASSERT_EQUAL_INT(SHROOM_REST_ROUTE_NOT_FOUND,
                        ShroomRestClassifyRoute("POST", "/health"));
  TEST_ASSERT_EQUAL_INT(SHROOM_REST_ROUTE_NOT_FOUND,
                        ShroomRestClassifyRoute("GET", "/health/extra"));
}

static void test_unknown_and_null_routes_are_not_found(void) {
  TEST_ASSERT_EQUAL_INT(SHROOM_REST_ROUTE_NOT_FOUND,
                        ShroomRestClassifyRoute("GET", "/v1/account/unknown"));
  TEST_ASSERT_EQUAL_INT(SHROOM_REST_ROUTE_NOT_FOUND, ShroomRestClassifyRoute(NULL, "/health"));
  TEST_ASSERT_EQUAL_INT(SHROOM_REST_ROUTE_NOT_FOUND, ShroomRestClassifyRoute("GET", NULL));
}

static void test_account_routes_require_contract_methods(void) {
  TEST_ASSERT_EQUAL_INT(SHROOM_REST_ROUTE_ACCOUNT_REGISTER,
                        ShroomRestClassifyRoute("POST", "/v1/account/register"));
  TEST_ASSERT_EQUAL_INT(SHROOM_REST_ROUTE_ACCOUNT_LOGIN,
                        ShroomRestClassifyRoute("POST", "/v1/account/login"));
  TEST_ASSERT_EQUAL_INT(SHROOM_REST_ROUTE_ACCOUNT_REFRESH,
                        ShroomRestClassifyRoute("POST", "/v1/account/refresh"));
  TEST_ASSERT_EQUAL_INT(SHROOM_REST_ROUTE_ACCOUNT_LOGOUT,
                        ShroomRestClassifyRoute("POST", "/v1/account/logout"));
  TEST_ASSERT_EQUAL_INT(SHROOM_REST_ROUTE_ACCOUNT_ME,
                        ShroomRestClassifyRoute("GET", "/v1/account/me"));
  TEST_ASSERT_EQUAL_INT(SHROOM_REST_ROUTE_NOT_FOUND,
                        ShroomRestClassifyRoute("GET", "/v1/account/login"));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_health_route_requires_exact_get_and_path);
  RUN_TEST(test_unknown_and_null_routes_are_not_found);
  RUN_TEST(test_account_routes_require_contract_methods);
  return UNITY_END();
}
