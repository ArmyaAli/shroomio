#include "screen.h"

#include "imgui_wrapper.h"
#include "raylib.h"

static bool CreditsInit(ShroomScreenManager* manager) {
  (void)manager;
  return true;
}

static void CreditsDraw(ShroomScreenManager* manager) {
  const int screen_width = GetScreenWidth();
  const int screen_height = GetScreenHeight();

  ClearBackground((Color){18, 20, 32, 255});

  ShroomImGui_SetNextWindowPos((screen_width - 460) * 0.5f, (screen_height - 300) * 0.5f,
                               SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(460.0f, 300.0f, SHROOM_IMGUI_COND_ALWAYS);
  if (!ShroomImGui_Begin("Credits", NULL,
                         SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                             SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_Text("shroomio");
  ShroomImGui_TextWrapped("A multiplayer arena prototype focused on readable movement, fast "
                          "iteration, and eventually a full Dear ImGui-driven client UI.");
  ShroomImGui_Separator();
  ShroomImGui_Text("Built with raylib, Dear ImGui, ENet, and SQLite.");
  ShroomImGui_TextWrapped("Thanks to the upstream open-source projects that make this kind of "
                          "iteration speed possible.");
  ShroomImGui_Spacing();

  if (ShroomImGui_Button("Back", 140.0f, 36.0f)) {
    ShroomScreenManagerGoBack(manager);
  }

  ShroomImGui_End();
}

static void CreditsHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerGoBack(manager);
  }
}

void ShroomScreenRegisterCredits(ShroomScreenManager* manager) {
  ShroomScreen* screen;

  if (manager == NULL) {
    return;
  }

  screen = &manager->screens[SHROOM_SCREEN_CREDITS];
  screen->type = SHROOM_SCREEN_CREDITS;
  screen->name = "Credits";
  screen->init = CreditsInit;
  screen->draw = CreditsDraw;
  screen->handle_input = CreditsHandleInput;
}
