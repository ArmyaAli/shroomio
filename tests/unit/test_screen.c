#include "unity.h"
#include "../src/client/screen.h"
#include <stddef.h>

static ShroomScreenManager manager;

static bool test_init_called = false;
static bool test_cleanup_called = false;

static bool TestScreenInit(ShroomScreenManager* mgr) {
  (void)mgr;
  test_init_called = true;
  return true;
}

static void TestScreenCleanup(ShroomScreenManager* mgr) {
  (void)mgr;
  test_cleanup_called = true;
}

void setUp(void) {
  ShroomScreenManagerInit(&manager);
  test_init_called = false;
  test_cleanup_called = false;
}

void tearDown(void) {
  ShroomScreenManagerShutdown(&manager);
}

void test_screen_manager_init(void) {
  TEST_ASSERT_TRUE(ShroomScreenManagerIsRunning(&manager));
  TEST_ASSERT_EQUAL(SHROOM_SCREEN_NONE, ShroomScreenManagerGetCurrentScreen(&manager));
}

void test_screen_manager_init_null(void) {
  ShroomScreenManagerInit(NULL);
}

void test_screen_manager_shutdown(void) {
  ShroomScreenManagerShutdown(&manager);
  TEST_ASSERT_FALSE(ShroomScreenManagerIsRunning(&manager));
}

void test_screen_manager_shutdown_null(void) {
  ShroomScreenManagerShutdown(NULL);
}

void test_screen_manager_transition_invalid_screen(void) {
  TEST_ASSERT_FALSE(ShroomScreenManagerTransition(&manager, SHROOM_SCREEN_NONE));
  TEST_ASSERT_FALSE(ShroomScreenManagerTransition(&manager, SHROOM_SCREEN_COUNT));
}

void test_screen_manager_transition_unregistered_screen(void) {
  TEST_ASSERT_FALSE(ShroomScreenManagerTransition(&manager, SHROOM_SCREEN_MAIN_MENU));
}

void test_screen_manager_transition_registered_screen(void) {
  ShroomScreen* screen = &manager.screens[SHROOM_SCREEN_MAIN_MENU];
  screen->type = SHROOM_SCREEN_MAIN_MENU;
  screen->name = "Test Menu";
  screen->init = TestScreenInit;
  screen->cleanup = TestScreenCleanup;
  
  TEST_ASSERT_TRUE(ShroomScreenManagerTransition(&manager, SHROOM_SCREEN_MAIN_MENU));
  TEST_ASSERT_TRUE(test_init_called);
  TEST_ASSERT_EQUAL(SHROOM_SCREEN_MAIN_MENU, ShroomScreenManagerGetCurrentScreen(&manager));
}

void test_screen_manager_transition_calls_cleanup(void) {
  ShroomScreen* screen1 = &manager.screens[SHROOM_SCREEN_MAIN_MENU];
  screen1->type = SHROOM_SCREEN_MAIN_MENU;
  screen1->name = "Menu 1";
  screen1->init = TestScreenInit;
  screen1->cleanup = TestScreenCleanup;
  
  ShroomScreen* screen2 = &manager.screens[SHROOM_SCREEN_SETTINGS];
  screen2->type = SHROOM_SCREEN_SETTINGS;
  screen2->name = "Settings";
  screen2->init = TestScreenInit;
  screen2->cleanup = TestScreenCleanup;
  
  ShroomScreenManagerTransition(&manager, SHROOM_SCREEN_MAIN_MENU);
  test_cleanup_called = false;
  
  ShroomScreenManagerTransition(&manager, SHROOM_SCREEN_SETTINGS);
  TEST_ASSERT_TRUE(test_cleanup_called);
  TEST_ASSERT_EQUAL(SHROOM_SCREEN_SETTINGS, ShroomScreenManagerGetCurrentScreen(&manager));
}

void test_screen_manager_go_back(void) {
  ShroomScreen* screen1 = &manager.screens[SHROOM_SCREEN_MAIN_MENU];
  screen1->type = SHROOM_SCREEN_MAIN_MENU;
  screen1->name = "Menu";
  screen1->init = TestScreenInit;
  screen1->cleanup = TestScreenCleanup;
  
  ShroomScreen* screen2 = &manager.screens[SHROOM_SCREEN_SETTINGS];
  screen2->type = SHROOM_SCREEN_SETTINGS;
  screen2->name = "Settings";
  screen2->init = TestScreenInit;
  screen2->cleanup = TestScreenCleanup;
  
  ShroomScreenManagerTransition(&manager, SHROOM_SCREEN_MAIN_MENU);
  ShroomScreenManagerTransition(&manager, SHROOM_SCREEN_SETTINGS);
  
  TEST_ASSERT_TRUE(ShroomScreenManagerGoBack(&manager));
  TEST_ASSERT_EQUAL(SHROOM_SCREEN_MAIN_MENU, ShroomScreenManagerGetCurrentScreen(&manager));
}

void test_screen_manager_go_back_no_previous(void) {
  TEST_ASSERT_FALSE(ShroomScreenManagerGoBack(&manager));
}

void test_screen_manager_request_exit(void) {
  TEST_ASSERT_TRUE(ShroomScreenManagerIsRunning(&manager));
  ShroomScreenManagerRequestExit(&manager);
  TEST_ASSERT_FALSE(ShroomScreenManagerIsRunning(&manager));
}

void test_screen_manager_request_exit_null(void) {
  ShroomScreenManagerRequestExit(NULL);
}

void test_screen_type_to_string(void) {
  TEST_ASSERT_EQUAL_STRING("NONE", ShroomScreenTypeToString(SHROOM_SCREEN_NONE));
  TEST_ASSERT_EQUAL_STRING("MAIN_MENU", ShroomScreenTypeToString(SHROOM_SCREEN_MAIN_MENU));
  TEST_ASSERT_EQUAL_STRING("SETTINGS", ShroomScreenTypeToString(SHROOM_SCREEN_SETTINGS));
  TEST_ASSERT_EQUAL_STRING("GAME", ShroomScreenTypeToString(SHROOM_SCREEN_GAME));
}

void test_screen_manager_get_current_screen_name(void) {
  ShroomScreen* screen = &manager.screens[SHROOM_SCREEN_MAIN_MENU];
  screen->type = SHROOM_SCREEN_MAIN_MENU;
  screen->name = "Main Menu";
  screen->init = TestScreenInit;
  
  ShroomScreenManagerTransition(&manager, SHROOM_SCREEN_MAIN_MENU);
  TEST_ASSERT_EQUAL_STRING("Main Menu", ShroomScreenManagerGetCurrentScreenName(&manager));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_screen_manager_init);
  RUN_TEST(test_screen_manager_init_null);
  RUN_TEST(test_screen_manager_shutdown);
  RUN_TEST(test_screen_manager_shutdown_null);
  RUN_TEST(test_screen_manager_transition_invalid_screen);
  RUN_TEST(test_screen_manager_transition_unregistered_screen);
  RUN_TEST(test_screen_manager_transition_registered_screen);
  RUN_TEST(test_screen_manager_transition_calls_cleanup);
  RUN_TEST(test_screen_manager_go_back);
  RUN_TEST(test_screen_manager_go_back_no_previous);
  RUN_TEST(test_screen_manager_request_exit);
  RUN_TEST(test_screen_manager_request_exit_null);
  RUN_TEST(test_screen_type_to_string);
  RUN_TEST(test_screen_manager_get_current_screen_name);
  return UNITY_END();
}
