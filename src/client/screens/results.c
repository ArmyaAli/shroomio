#include "client/game.h"
#include "client/layout.h"
#include "client/results_summary.h"
#include "client/screen.h"
#include "client/screens/screen_background.h"
#include "imgui_wrapper.h"
#include "raylib.h"

#include <stdio.h>

static bool ResultsInit(ShroomScreenManager* manager) {
  (void)manager;
  return true;
}

static void ResultsReconnectOnline(ShroomScreenManager* manager, Game* game) {
  char host[sizeof(game->selected_server_host)];
  const uint16_t port = game->selected_server_port;

  snprintf(host, sizeof(host), "%s", game->selected_server_host);
  if (!ShroomScreenManagerTransition(manager, SHROOM_SCREEN_LOBBY)) {
    return;
  }

  ClientNetInit(&game->net, host, port);
  game->auto_join_lobby = true;
}

static bool IsAuthoritativeOnlineResults(const Game* game) {
  return game->authoritative_round_resume_pending && game->net.welcome_received &&
         ((game->active_mode == SHROOM_SESSION_MODE_QUICK_PLAY) ||
          (game->active_mode == SHROOM_SESSION_MODE_LOBBY_PLAY));
}

static void ResultsUpdate(ShroomScreenManager* manager, float delta_time) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  if ((game == NULL) || !IsAuthoritativeOnlineResults(game)) {
    return;
  }

  GameUpdate(game, delta_time);
  if (game->world.match_phase == SHROOM_MATCH_PHASE_RUNNING) {
    game->show_results = false;
    game->session_start_time = (float)GetTime();
    game->session_duration_seconds = 0u;
    game->authoritative_round_resume_pending = true;
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
  }
}

static void ResultsDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  if (game == NULL) {
    return;
  }

  ShroomScreenDrawFungalBackground(game->settings.menu_animations_enabled);

  if (!ShroomLayoutBeginCenteredPanel("Match Results", 500.0f, 400.0f, 0.88f,
                                      SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                                          SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                                          SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomLayoutHeading("Session Summary");

  char duration_value[32];
  char duration_text[64];
  ShroomResultsFormatDuration(game->session_duration_seconds, duration_value,
                              sizeof(duration_value));
  snprintf(duration_text, sizeof(duration_text), "Duration: %s", duration_value);
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

  const float button_width = ShroomLayoutMetric(200.0f);
  const float button_height = ShroomLayoutMetric(40.0f);

  if (IsAuthoritativeOnlineResults(game)) {
    ShroomImGui_Text("Waiting for the next round...");
  } else {
    ShroomImGui_SetNextItemWidth(button_width);
    if (ShroomImGui_Button("Play Again", button_width, button_height)) {
      GamePlayUiClickSound(game);
      game->show_results = false;
      if ((game->active_mode == SHROOM_SESSION_MODE_LOBBY_PLAY) &&
          ClientNetCanResumeLobbySession(&game->net)) {
        game->resume_online_session_requested = true;
        ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
      } else if (game->active_mode == SHROOM_SESSION_MODE_OFFLINE_PRACTICE) {
        ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
      } else {
        ResultsReconnectOnline(manager, game);
      }
    }
    ShroomImGui_SameLine();
  }

  ShroomImGui_SetNextItemWidth(button_width);
  if (ShroomImGui_Button("Main Menu", button_width, button_height)) {
    GamePlayUiClickSound(game);
    game->show_results = false;
    if (IsAuthoritativeOnlineResults(game)) {
      game->authoritative_round_resume_pending = false;
      GameShutdown(game);
    }
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_MAIN_MENU);
  }

  ShroomImGui_End();
}

static void ResultsHandleInput(ShroomScreenManager* manager) { (void)manager; }

static void ResultsCleanup(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  if (game == NULL) {
    return;
  }
  if (game->authoritative_round_resume_pending) {
    return;
  }
  if (game->resume_online_session_requested) {
    game->resume_online_session_requested = false;
    return;
  }

  GameShutdown(game);
}

#ifdef TEST_MODE
const char* ShroomTestGetResultsDurationText(const Game* game) {
  static char duration_text[32];

  if (game == NULL) {
    return "";
  }

  ShroomResultsFormatDuration(game->session_duration_seconds, duration_text, sizeof(duration_text));
  return duration_text;
}
#endif

void ShroomScreenRegisterResults(ShroomScreenManager* manager) {
  ShroomScreen* screen;

  if (manager == NULL) {
    return;
  }

  screen = &manager->screens[SHROOM_SCREEN_RESULTS];
  screen->type = SHROOM_SCREEN_RESULTS;
  screen->name = "Results";
  screen->init = ResultsInit;
  screen->update = ResultsUpdate;
  screen->draw = ResultsDraw;
  screen->handle_input = ResultsHandleInput;
  screen->cleanup = ResultsCleanup;
}
