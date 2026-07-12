#include "client/game.h"
#include "client/imgui_wrapper.h"
#include "client/layout.h"
#include "client/net.h"
#include "client/screen.h"
#include "client/screens/screen_background.h"

#include <math.h>

#include "raylib.h"

static float g_status_pulse_timer;

typedef enum LobbyRosterStatus {
  LOBBY_ROSTER_STATUS_WAITING = 0,
  LOBBY_ROSTER_STATUS_STARTING_SOON,
  LOBBY_ROSTER_STATUS_IN_PROGRESS,
} LobbyRosterStatus;

static LobbyRosterStatus LobbyRosterComputeStatus(const Game *game) {
  if (game == NULL) {
    return LOBBY_ROSTER_STATUS_WAITING;
  }
  if (game->net.lobby_match_started) {
    return LOBBY_ROSTER_STATUS_IN_PROGRESS;
  }
  for (uint16_t i = 0; i < game->net.lobby_roster_count; ++i) {
    if (game->net.lobby_roster[i].player_id == game->net.player_id &&
        game->net.lobby_roster[i].is_ready != 0u) {
      return LOBBY_ROSTER_STATUS_STARTING_SOON;
    }
  }
  return LOBBY_ROSTER_STATUS_WAITING;
}

static const char *LobbyRosterStatusLabel(LobbyRosterStatus status) {
  switch (status) {
  case LOBBY_ROSTER_STATUS_STARTING_SOON:
    return "Starting Soon";
  case LOBBY_ROSTER_STATUS_IN_PROGRESS:
    return "In Progress";
  case LOBBY_ROSTER_STATUS_WAITING:
  default:
    return "Waiting";
  }
}

static bool LobbyRosterInit(ShroomScreenManager *manager) {
  const Game *game = manager != NULL ? (const Game *)manager->user_data : NULL;

  if (game == NULL) {
    return false;
  }
  g_status_pulse_timer = 0.0f;
  return true;
}

static void LobbyRosterUpdate(ShroomScreenManager *manager, float delta_time) {
  Game *game = manager != NULL ? (Game *)manager->user_data : NULL;
  const ShroomVec2 no_input = {0};

  if (game == NULL) {
    return;
  }

  /* Keep pumping ENet so LOBBY_JOINED and snapshots continue to flow
   * while the player reviews the roster. We do NOT auto-transition to
   * gameplay — entering the match is explicit via the Enter Match button. */
  ClientNetUpdate(&game->net, no_input, false, false, no_input, 0u, delta_time);

  g_status_pulse_timer += delta_time;
}

static void LobbyRosterDraw(ShroomScreenManager *manager) {
  Game *game = manager != NULL ? (Game *)manager->user_data : NULL;
  const ShroomLobbyRosterEntry *local_entry = NULL;

  if (game == NULL) {
    return;
  }
  for (uint16_t i = 0; i < game->net.lobby_roster_count; ++i) {
    if (game->net.lobby_roster[i].player_id == game->net.player_id) {
      local_entry = &game->net.lobby_roster[i];
      break;
    }
  }

  ShroomScreenDrawFungalBackground(game->settings.menu_animations_enabled);

  if (!ShroomLayoutBeginCenteredPanel(
          "Lobby Roster", 560.0f, 420.0f, 0.9f,
          SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
              SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
              SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_Text("Lobby:");
  ShroomImGui_SameLine();
  ShroomImGui_TextColored(
      (ShroomImGuiColor){0.4f, 1.0f, 0.6f, 1.0f},
      (game->net.lobby_name[0] != '\0') ? game->net.lobby_name : "Arena");
  ShroomImGui_SameLine();
  ShroomImGui_TextDisabled(TextFormat("(id %u)", game->net.lobby_id));

  /* Capacity line refreshes every frame from the live snapshot count. */
  const uint16_t current = game->net.lobby_roster_count;
  const uint16_t capacity = (game->net.lobby_max_players > 0u)
                                ? game->net.lobby_max_players
                                : (uint16_t)SHROOM_MAX_PLAYERS;
  ShroomImGui_TextDisabled(
      TextFormat("%u / %u players in match", current, capacity));

  ShroomImGui_Separator();

  if (ShroomImGui_BeginTable("RosterTable", 3,
                             SHROOM_IMGUI_TABLE_BORDERS |
                                 SHROOM_IMGUI_TABLE_ROW_BG |
                                 SHROOM_IMGUI_TABLE_SIZING_FIXED,
                             0.0f, ShroomLayoutMetric(220.0f))) {
    ShroomImGui_TableSetupColumn("Player", ShroomLayoutMetric(280.0f));
    ShroomImGui_TableSetupColumn("Role", ShroomLayoutMetric(120.0f));
    ShroomImGui_TableSetupColumn("Status", ShroomLayoutMetric(120.0f));
    ShroomImGui_TableHeadersRow();

    /* Local player row is always first and highlighted. */
    ShroomImGui_TableNextRow();
    ShroomImGui_TableSetColumnIndex(0);
    ShroomImGui_TextColored((ShroomImGuiColor){1.0f, 0.85f, 0.3f, 1.0f},
                            TextFormat("You (id %u)", game->net.player_id));
    ShroomImGui_TableSetColumnIndex(1);
    ShroomImGui_Text(game->net.spectating ? "Spectator" : "Player");
    ShroomImGui_TableSetColumnIndex(2);
    ShroomImGui_Text(local_entry == NULL
                         ? "Waiting for status"
                         : (local_entry->is_ready ? "Ready" : "Not Ready"));

    /* Peers sourced from the latest snapshot; the list refreshes every
     * frame as players join and leave. Bots auto-ready so the lobby reads
     * as live while the local player decides. */
    if (game->net.snapshot_player_count > 0u) {
      uint16_t i;
      for (i = 0u; i < game->net.snapshot_player_count; ++i) {
        const ShroomSnapshotPlayerState *peer = &game->net.snapshot_players[i];
        if (peer->player_id == game->net.player_id) {
          continue;
        }
        ShroomImGui_TableNextRow();
        ShroomImGui_TableSetColumnIndex(0);
        ShroomImGui_Text(TextFormat("Player %u", peer->player_id));
        ShroomImGui_TableSetColumnIndex(1);
        ShroomImGui_Text(peer->is_bot ? "Bot" : "Player");
        ShroomImGui_TableSetColumnIndex(2);
        if (peer->is_bot) {
          ShroomImGui_Text("Ready");
        } else {
          const ShroomLobbyRosterEntry *roster_entry = NULL;
          for (uint16_t j = 0; j < game->net.lobby_roster_count; ++j) {
            if (game->net.lobby_roster[j].player_id == peer->player_id) {
              roster_entry = &game->net.lobby_roster[j];
              break;
            }
          }
          ShroomImGui_Text(
              roster_entry == NULL
                  ? "Waiting for status"
                  : (roster_entry->is_ready ? "Ready" : "Not Ready"));
        }
      }
    } else {
      ShroomImGui_TableNextRow();
      ShroomImGui_TableSetColumnIndex(0);
      ShroomImGui_TextDisabled("Waiting for peers...");
    }
    ShroomImGui_EndTable();
  }

  ShroomImGui_Separator();

  const LobbyRosterStatus status = LobbyRosterComputeStatus(game);
  const char *status_label = LobbyRosterStatusLabel(status);
  const float pulse = 0.5f + 0.5f * sinf(g_status_pulse_timer * 4.0f);
  const ShroomImGuiColor status_color =
      (status == LOBBY_ROSTER_STATUS_IN_PROGRESS)
          ? (ShroomImGuiColor){0.4f, 1.0f, 0.5f, 1.0f}
          : (ShroomImGuiColor){pulse, pulse, 0.9f, 1.0f};
  ShroomImGui_Text("Match Status:");
  ShroomImGui_SameLine();
  ShroomImGui_TextColored(status_color, status_label);

  ShroomImGui_Spacing();

  bool desired_ready = local_entry != NULL && local_entry->is_ready != 0u;
  if (!game->net.spectating &&
      ShroomImGui_Checkbox("Ready to enter", &desired_ready)) {
    ClientNetSendReadyState(&game->net, desired_ready);
  }
  ShroomImGui_SameLine();
  if (ShroomImGui_Button("Leave Lobby", ShroomLayoutMetric(140.0f), 0.0f)) {
    ClientNetSendLobbyLeave(&game->net);
    ClientNetShutdown(&game->net);
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }

  ShroomImGui_Spacing();

  /* Entering the match is explicit — the player stays on the roster until
   * they choose to enter. The server is already sending snapshots, so we
   * just need to flip the session mode and transition. */
  const bool can_enter =
      game->net.spectating ||
      (game->net.welcome_received && game->net.lobby_roster_received &&
       local_entry != NULL && local_entry->is_ready != 0u);
  if (can_enter) {
    if (ShroomImGui_Button("Enter Match", ShroomLayoutMetric(140.0f), 0.0f)) {
      ClientNetSendEnterMatch(&game->net);
      game->selected_mode = SHROOM_SESSION_MODE_LOBBY_PLAY;
      game->active_mode = SHROOM_SESSION_MODE_LOBBY_PLAY;
      ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
    }
  } else {
    ShroomImGui_TextDisabled(game->net.lobby_roster_received
                                 ? "Ready up to enter"
                                 : "Waiting for server...");
  }

  ShroomImGui_End();
}

static void LobbyRosterHandleInput(ShroomScreenManager *manager) {
  Game *game = manager != NULL ? (Game *)manager->user_data : NULL;

  if (game == NULL) {
    return;
  }

  if (!ShroomImGui_WantCaptureKeyboard() && IsKeyPressed(KEY_ESCAPE)) {
    ClientNetSendLobbyLeave(&game->net);
    ClientNetShutdown(&game->net);
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }
}

static void LobbyRosterCleanup(ShroomScreenManager *manager) {
  (void)manager;
  g_status_pulse_timer = 0.0f;
}

void ShroomScreenRegisterLobbyRoster(ShroomScreenManager *manager) {
  ShroomScreen *screen;

  if (manager == NULL) {
    return;
  }

  screen = &manager->screens[SHROOM_SCREEN_LOBBY_ROSTER];
  screen->type = SHROOM_SCREEN_LOBBY_ROSTER;
  screen->name = "Lobby Roster";
  screen->init = LobbyRosterInit;
  screen->update = LobbyRosterUpdate;
  screen->draw = LobbyRosterDraw;
  screen->handle_input = LobbyRosterHandleInput;
  screen->cleanup = LobbyRosterCleanup;
}
