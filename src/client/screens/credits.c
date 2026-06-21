#include "game.h"
#include "layout.h"
#include "screen.h"
#include "screen_background.h"

#include "imgui_wrapper.h"
#include "raylib.h"

static bool CreditsInit(ShroomScreenManager* manager) {
  (void)manager;
  return true;
}

static void CreditsDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  ShroomScreenDrawFungalBackground((game == NULL) || game->settings.menu_animations_enabled);

  if (!ShroomLayoutBeginCenteredPanel("Credits", 460.0f, 300.0f, 0.88f,
                                      SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                                          SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                                          SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomLayoutHeading("shroomio");
  ShroomImGui_TextWrapped("A multiplayer arena prototype focused on readable movement, fast "
                          "iteration, and eventually a full Dear ImGui-driven client UI.");
  ShroomImGui_Text("Built with raylib, Dear ImGui, ENet, and SQLite.");
  ShroomImGui_TextWrapped("Thanks to the upstream open-source projects that make this kind of "
                          "iteration speed possible.");
  ShroomImGui_Spacing();

  if (ShroomImGui_Button("Back", 140.0f, 36.0f)) {
    GamePlayUiClickSound(game);
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
