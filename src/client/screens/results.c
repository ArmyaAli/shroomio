#include "client/game.h"
#include "client/layout.h"
#include "client/results_summary.h"
#include "client/results_transition.h"
#include "client/screen.h"
#include "client/screens/screen_background.h"
#include "client/voice.h"
#include "imgui_wrapper.h"
#include "raylib.h"
#include "shared/sim.h"

#include <stdio.h>

static bool ResultsInit(ShroomScreenManager* manager) {
  (void)manager;
  return true;
}

static void FormatResultsSpores(const Game* game, char* text, size_t text_size) {
  snprintf(text, text_size, "Spores Collected: %u", game->final_spores_collected);
}

static void FormatResultsKills(const Game* game, char* text, size_t text_size) {
  snprintf(text, text_size, "Players Consumed: %u", game->final_kills);
}

static void ResultsReconnectOnline(ShroomScreenManager* manager, Game* game) {
  char host[sizeof(game->selected_server_host)];
  const uint16_t port = game->selected_server_port;

  snprintf(host, sizeof(host), "%s", game->selected_server_host);
  if (!ShroomScreenManagerTransition(manager, SHROOM_SCREEN_LOBBY)) {
    return;
  }

  ClientNetInit(&game->net, host, port, game->settings.player_name);
  game->auto_join_lobby = true;
}

static bool IsAuthoritativeOnlineResults(const Game* game) {
  return game->authoritative_round_resume_pending && game->net.welcome_received &&
         ((game->active_mode == SHROOM_SESSION_MODE_QUICK_PLAY) ||
          (game->active_mode == SHROOM_SESSION_MODE_LOBBY_PLAY));
}

static bool ResultsVoteButton(const char* label, bool selected, float width, float height) {
  bool clicked;

  if (selected) {
    ShroomImGui_PushStyleColor(SHROOM_IMGUI_COL_BUTTON, 0.24f, 0.36f, 0.16f, 0.92f);
    ShroomImGui_PushStyleColor(SHROOM_IMGUI_COL_BUTTON_HOVERED, 0.34f, 0.50f, 0.23f, 0.96f);
    ShroomImGui_PushStyleColor(SHROOM_IMGUI_COL_BUTTON_ACTIVE, 0.42f, 0.62f, 0.28f, 1.0f);
  }
  clicked = ShroomImGui_Button(label, width, height);
  if (selected) {
    ShroomImGui_PopStyleColor();
    ShroomImGui_PopStyleColor();
    ShroomImGui_PopStyleColor();
  }
  return clicked;
}

static void ResultsUpdate(ShroomScreenManager* manager, float delta_time) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  ShroomResultsRoute route;

  if ((game == NULL) || !IsAuthoritativeOnlineResults(game)) {
    return;
  }

  GameUpdate(game, delta_time);
  route = ShroomResultsResolveRoute(
      game->world.match_phase, game->net.intermission_received, &game->net.intermission,
      game->net.consumed_intermission_round_valid, game->net.consumed_intermission_round_id);
  if ((route == SHROOM_RESULTS_ROUTE_LOBBY) || (route == SHROOM_RESULTS_ROUTE_SPECTATE)) {
    ShroomVoiceSetSessionActive(false);
    game->net.spectating = route == SHROOM_RESULTS_ROUTE_SPECTATE;
    ClientNetConsumeIntermission(&game->net);
    game->authoritative_round_resume_pending = false;
    game->resume_online_session_requested = true;
    game->show_results = false;
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_LOBBY);
    return;
  }
  if (route == SHROOM_RESULTS_ROUTE_GAME) {
    ClientNetConsumeIntermission(&game->net);
    game->show_results = false;
    game->session_start_time = (float)GetTime();
    game->session_duration_seconds = 0u;
    game->peak_mass = game->local_player != NULL
                          ? ShroomWorldGetColonyMass(&game->world, game->local_player->player_id)
                          : 0.0f;
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

  if (!ShroomLayoutBeginCenteredPanel("Match Results", 500.0f, 460.0f, 0.88f,
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

  char spores_text[64];
  FormatResultsSpores(game, spores_text, sizeof(spores_text));
  ShroomImGui_Text(spores_text);

  char kills_text[64];
  FormatResultsKills(game, kills_text, sizeof(kills_text));
  ShroomImGui_Text(kills_text);

  if (game->final_rank > 0) {
    char final_rank_text[64];
    snprintf(final_rank_text, sizeof(final_rank_text), "Final Rank: #%d", game->final_rank);
    ShroomImGui_Text(final_rank_text);
  } else {
    ShroomImGui_Text("Final Rank: N/A");
  }

  if (game->world.game_mode == SHROOM_GAME_MODE_KING_OF_HILL) {
    ShroomImGui_Text(TextFormat("Hill Score: %.1f / %.0f",
                                ShroomWorldGetObjectiveScore(&game->world, game->net.player_id),
                                game->world.objective_target_score));
  }

  ShroomImGui_Spacing();
  ShroomImGui_Spacing();

  const float button_width = ShroomLayoutMetric(200.0f);
  const float button_height = ShroomLayoutMetric(40.0f);

  if (IsAuthoritativeOnlineResults(game)) {
    char countdown[64];
    char local_vote[64];
    char participation[64];
    char totals[96];
    const ShroomIntermissionStatusPacket* status = &game->net.intermission;

    if (status->resolved) {
      ShroomResultsFormatDecision((ShroomRematchVote)status->decision, countdown,
                                  sizeof(countdown));
    } else {
      snprintf(countdown, sizeof(countdown), "Vote ends in %.0f seconds",
               status->seconds_remaining);
    }
    ShroomResultsFormatLocalVote((ShroomRematchVote)status->your_vote, status->can_vote != 0u,
                                 local_vote, sizeof(local_vote));
    ShroomResultsFormatVoteParticipation(status->play_again_votes, status->return_to_lobby_votes,
                                         status->spectate_votes, status->eligible_count,
                                         participation, sizeof(participation));
    snprintf(totals, sizeof(totals), "Play Again %u  Lobby %u  Spectate %u",
             status->play_again_votes, status->return_to_lobby_votes, status->spectate_votes);
    ShroomImGui_Text(countdown);
    ShroomImGui_Text(local_vote);
    ShroomImGui_Text(participation);
    ShroomImGui_Text(totals);
    if (status->can_vote && !status->resolved) {
      const bool play_again_selected = status->your_vote == SHROOM_REMATCH_VOTE_PLAY_AGAIN;
      const bool lobby_selected = status->your_vote == SHROOM_REMATCH_VOTE_RETURN_TO_LOBBY;
      const bool spectate_selected = status->your_vote == SHROOM_REMATCH_VOTE_SPECTATE;

      if (ResultsVoteButton(play_again_selected ? "Vote Play Again (Selected)" : "Vote Play Again",
                            play_again_selected, button_width, button_height)) {
        ClientNetSendRematchVote(&game->net, SHROOM_REMATCH_VOTE_PLAY_AGAIN);
      }
      ShroomImGui_SameLine();
      if (ResultsVoteButton(lobby_selected ? "Vote Lobby (Selected)" : "Vote Lobby", lobby_selected,
                            button_width, button_height)) {
        ClientNetSendRematchVote(&game->net, SHROOM_REMATCH_VOTE_RETURN_TO_LOBBY);
      }
      if (ResultsVoteButton(spectate_selected ? "Vote Spectate (Selected)" : "Vote Spectate",
                            spectate_selected, button_width, button_height)) {
        ClientNetSendRematchVote(&game->net, SHROOM_REMATCH_VOTE_SPECTATE);
      }
    } else if (!status->resolved) {
      ShroomImGui_Text("Spectators and late joiners observe this vote.");
    }
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

const char* ShroomTestGetResultsSporesText(const Game* game) {
  static char spores_text[64];

  if (game == NULL) {
    return "";
  }
  FormatResultsSpores(game, spores_text, sizeof(spores_text));
  return spores_text;
}

const char* ShroomTestGetResultsKillsText(const Game* game) {
  static char kills_text[64];

  if (game == NULL) {
    return "";
  }
  FormatResultsKills(game, kills_text, sizeof(kills_text));
  return kills_text;
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
