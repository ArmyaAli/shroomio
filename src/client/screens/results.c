#include "client/game.h"
#include "client/screen.h"
#include "client/screens/screen_background.h"
#include "imgui_wrapper.h"
#include "raylib.h"

#include <stdio.h>

static bool ResultsInit(ShroomScreenManager* manager) {
  (void)manager;
  return true;
}

static void ResultsDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  const int screen_width = GetScreenWidth();
  const int screen_height = GetScreenHeight();

  if (game == NULL) {
    return;
  }

  ShroomScreenDrawFungalBackground(game->settings.menu_animations_enabled);

  ShroomImGui_SetNextWindowPos((screen_width - 500.0f) * 0.5f, (screen_height - 400.0f) * 0.5f,
                               SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(500.0f, 400.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.88f);
  if (!ShroomImGui_Begin("Match Results", NULL,
                         SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                             SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  const float session_duration = (float)GetTime() - game->session_start_time;
  const int minutes = (int)session_duration / 60;
  const int seconds = (int)session_duration % 60;

  ShroomImGui_Text("Session Summary");
  ShroomImGui_Separator();
  ShroomImGui_Spacing();

  char duration_text[64];
  snprintf(duration_text, sizeof(duration_text), "Duration: %d:%02d", minutes, seconds);
  ShroomImGui_Text(duration_text);

  char peak_mass_text[64];
  snprintf(peak_mass_text, sizeof(peak_mass_text), "Peak Mass: %.0f", game->peak_mass);
  ShroomImGui_Text(peak_mass_text);

  char final_mass_text[64];
  snprintf(final_mass_text, sizeof(final_mass_text), "Final Mass: %.0f", game->final_mass);
  ShroomImGui_Text(final_mass_text);

  if (game->final_rank > 0) {
    char final_rank_text[64];
    snprintf(final_rank_text, sizeof(final_rank_text), "Final Rank: #%d", game->final_rank);
    ShroomImGui_Text(final_rank_text);
  } else {
    ShroomImGui_Text("Final Rank: N/A");
  }

  ShroomImGui_Spacing();
  ShroomImGui_Spacing();

  const float button_width = 200.0f;
  const float button_height = 40.0f;

  ShroomImGui_SetNextItemWidth(button_width);
  if (ShroomImGui_Button("Play Again", button_width, button_height)) {
    GamePlayUiClickSound(game);
    game->show_results = false;
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }

  ShroomImGui_SameLine();
  ShroomImGui_SetNextItemWidth(button_width);
  if (ShroomImGui_Button("Main Menu", button_width, button_height)) {
    GamePlayUiClickSound(game);
    game->show_results = false;
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_MAIN_MENU);
  }

  ShroomImGui_End();
}

static void ResultsHandleInput(ShroomScreenManager* manager) { (void)manager; }

void ShroomScreenRegisterResults(ShroomScreenManager* manager) {
  ShroomScreen* screen;

  if (manager == NULL) {
    return;
  }

  screen = &manager->screens[SHROOM_SCREEN_RESULTS];
  screen->type = SHROOM_SCREEN_RESULTS;
  screen->name = "Results";
  screen->init = ResultsInit;
  screen->draw = ResultsDraw;
  screen->handle_input = ResultsHandleInput;
}
