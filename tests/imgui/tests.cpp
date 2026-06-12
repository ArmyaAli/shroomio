#include "app.h"

#include "imgui_test_engine/imgui_te_context.h"

void ShroomRegisterImGuiTests(ImGuiTestEngine* engine) {
  ImGuiTest* test;

  test = IM_REGISTER_TEST(engine, "screens", "main_menu_navigation");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ShroomImGuiTestAppReset(true);

    ctx->SetRef("Main Menu");
    ctx->ItemClick("Settings");
    IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
                SHROOM_SCREEN_SETTINGS);

    ctx->SetRef("Settings");
    ctx->ItemClick("Back");
    IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
                SHROOM_SCREEN_MAIN_MENU);

    ctx->SetRef("Main Menu");
    ctx->ItemClick("Help");
    IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
                SHROOM_SCREEN_HELP);
  };

  test = IM_REGISTER_TEST(engine, "screens", "settings_persistence");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ClientSettings loaded = {};

    ShroomImGuiTestAppReset(true);

    ctx->SetRef("Main Menu");
    ctx->ItemClick("Settings");
    ctx->SetRef("Settings");
    ctx->ItemInputValue("UI Scale", 130);
    ctx->ItemCheck("Invert Mouse");
    ctx->ItemCheck("Show Diagnostics On Launch");
    ctx->ItemClick("Save");

    IM_CHECK(ClientSettingsLoad(&loaded));
    IM_CHECK_EQ(loaded.ui_scale_percent, 130);
    IM_CHECK(loaded.invert_mouse);
    IM_CHECK(loaded.diagnostics_enabled);
  };

  test = IM_REGISTER_TEST(engine, "screens", "server_browser_join_and_validation");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ShroomImGuiTestAppReset(true);

    ctx->SetRef("Main Menu");
    ctx->ItemClick("Server Browser");
    IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
                SHROOM_SCREEN_SERVER_BROWSER);

    ctx->SetRef("Server Browser");
    ctx->ItemInputValue("Port", "70000");
    ctx->ItemClick("Join Host");
    IM_CHECK_STR_EQ(ShroomTestGetServerBrowserValidationMessage(),
                    "Port must be between 1 and 65535.");

    ctx->ItemInputValue("Port", "7777");
    IM_CHECK_EQ(ShroomTestGetServerBrowserSelectedIndex(), 0);
    ctx->ItemClick("Join Selected");

    IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
                SHROOM_SCREEN_GAME);
    IM_CHECK_STR_EQ(g_imgui_test_app.game.selected_server_host, "127.0.0.1");
    IM_CHECK_EQ(g_imgui_test_app.game.selected_server_port, SHROOM_SERVER_PORT);
  };
}
