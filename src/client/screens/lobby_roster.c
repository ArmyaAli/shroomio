#include "client/game.h"
#include "client/imgui_wrapper.h"
#include "client/net.h"
#include "client/screen.h"
#include "client/screens/screen_background.h"

#include <math.h>

#include "raylib.h"

/* Placeholder roster state — the server does not yet broadcast per-lobby
 * rosters, so we approximate with snapshot players (once the first
 * snapshot arrives) and surface local ready state locally. This keeps
 * visibility, leave, and countdown messaging atomic and testable. */
static bool g_ready_state;
static float g_status_pulse_timer;

typedef enum LobbyRosterStatus {
  LOBBY_ROSTER_STATUS_WAITING = 0,
  LOBBY_ROSTER_STATUS_STARTING_SOON,
  LOBBY_ROSTER_STATUS_IN_PROGRESS,
} LobbyRosterStatus;

static LobbyRosterStatus LobbyRosterComputeStatus(const Game* game) {
  if (game == NULL) {
    return LOBBY_ROSTER_STATUS_WAITING;
  }
  /* Once we receive the welcome/snapshot, the match has started. */
  if (game->net.welcome_received && game->net.last_snapshot_tick != 0ull) {
    return LOBBY_ROSTER_STATUS_IN_PROGRESS;
  }
  /* Starting-soon placeholder: local ready and at least one peer snapshot
   * has player data queued. */
  if (g_ready_state && game->net.snapshot_player_count > 0u) {
    return LOBBY_ROSTER_STATUS_STARTING_SOON;
  }
  return LOBBY_ROSTER_STATUS_WAITING;
}

static const char* LobbyRosterStatusLabel(LobbyRosterStatus status) {
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

static bool LobbyRosterInit(ShroomScreenManager* manager) {
  const Game* game = manager != NULL ? (const Game*)manager->user_data : NULL;

  if (game == NULL) {
    return false;
  }
  g_ready_state = false;
  g_status_pulse_timer = 0.0f;
  return true;
}

static void LobbyRosterUpdate(ShroomScreenManager* manager, float delta_time) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  const ShroomVec2 no_input = {0};

  if (game == NULL) {
    return;
  }

  /* Keep pumping ENet so LOBBY_JOINED and snapshots continue to flow
   * while the player reviews the roster. We do NOT auto-transition to
   * gameplay — entering the match is explicit via the Enter Match button. */
  ClientNetUpdate(&game->net, no_input, false, no_input, 0u, delta_time);

  g_status_pulse_timer += delta_time;
}

static void LobbyRosterDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  if (game == NULL) {
    return;
  }

  ShroomScreenDrawFungalBackground(game->settings.menu_animations_enabled);

  const float w = 560.0f;
  const float h = 420.0f;
  ShroomImGui_SetNextWindowPos((GetScreenWidth() - w) * 0.5f, (GetScreenHeight() - h) * 0.5f,
                               SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(w, h, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.9f);

  if (!ShroomImGui_Begin("Lobby Roster", NULL,
                         SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                             SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS |
                             SHROOM_IMGUI_WINDOW_NO_SCROLLBAR)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_Text("Lobby:");
  ShroomImGui_SameLine();
  ShroomImGui_TextColored((ShroomImGuiColor){0.4f, 1.0f, 0.6f, 1.0f},
                          (game->net.lobby_name[0] != '\0') ? game->net.lobby_name : "Arena");
  ShroomImGui_SameLine();
  ShroomImGui_TextDisabled(TextFormat("(id %u)", game->net.lobby_id));

  /* Capacity line refreshes every frame from the live snapshot count. */
  const uint16_t current = game->net.snapshot_player_count;
  const uint16_t capacity = (game->net.lobby_max_players > 0u) ? game->net.lobby_max_players
                                                               : (uint16_t)SHROOM_MAX_PLAYERS;
  ShroomImGui_TextDisabled(TextFormat("%u / %u players in match", current, capacity));

  ShroomImGui_Separator();

  if (ShroomImGui_BeginTable("RosterTable", 3,
                             SHROOM_IMGUI_TABLE_BORDERS | SHROOM_IMGUI_TABLE_ROW_BG |
                                 SHROOM_IMGUI_TABLE_SIZING_FIXED,
                             530.0f, 220.0f)) {
    ShroomImGui_TableSetupColumn("Player", 280.0f);
    ShroomImGui_TableSetupColumn("Role", 120.0f);
    ShroomImGui_TableSetupColumn("Status", 120.0f);
    ShroomImGui_TableHeadersRow();

    /* Local player row is always first and highlighted. */
    ShroomImGui_TableNextRow();
    ShroomImGui_TableSetColumnIndex(0);
    ShroomImGui_TextColored((ShroomImGuiColor){1.0f, 0.85f, 0.3f, 1.0f},
                            TextFormat("You (id %u)", game->net.player_id));
    ShroomImGui_TableSetColumnIndex(1);
    ShroomImGui_Text(game->net.spectating ? "Spectator" : "Player");
    ShroomImGui_TableSetColumnIndex(2);
    ShroomImGui_Text(g_ready_state ? "Ready" : "Not Ready");

    /* Peers sourced from the latest snapshot; the list refreshes every
     * frame as players join and leave. Bots auto-ready so the lobby reads
     * as live while the local player decides. */
    if (game->net.snapshot_player_count > 0u) {
      uint16_t i;
      for (i = 0u; i < game->net.snapshot_player_count; ++i) {
        const ShroomSnapshotPlayerState* peer = &game->net.snapshot_players[i];
        if (peer->player_id == game->net.player_id) {
          continue;
        }
        ShroomImGui_TableNextRow();
        ShroomImGui_TableSetColumnIndex(0);
        ShroomImGui_Text(TextFormat("Player %u", peer->player_id));
        ShroomImGui_TableSetColumnIndex(1);
        ShroomImGui_Text(peer->is_bot ? "Bot" : "Player");
        ShroomImGui_TableSetColumnIndex(2);
        ShroomImGui_Text(peer->is_bot ? "Ready" : "Not Ready");
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
  const char* status_label = LobbyRosterStatusLabel(status);
  const float pulse = 0.5f + 0.5f * sinf(g_status_pulse_timer * 4.0f);
  const ShroomImGuiColor status_color = (status == LOBBY_ROSTER_STATUS_IN_PROGRESS)
                                            ? (ShroomImGuiColor){0.4f, 1.0f, 0.5f, 1.0f}
                                            : (ShroomImGuiColor){pulse, pulse, 0.9f, 1.0f};
  ShroomImGui_Text("Match Status:");
  ShroomImGui_SameLine();
  ShroomImGui_TextColored(status_color, status_label);

  ShroomImGui_Spacing();

  if (ShroomImGui_Button(g_ready_state ? "Ready" : "Not Ready", 140.0f, 0.0f)) {
    g_ready_state = !g_ready_state;
  }
  ShroomImGui_SameLine();
  if (ShroomImGui_Button("Leave Lobby", 140.0f, 0.0f)) {
    ClientNetSendLobbyLeave(&game->net);
    ClientNetShutdown(&game->net);
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }

  ShroomImGui_Spacing();

  /* Entering the match is explicit — the player stays on the roster until
   * they choose to enter. The server is already sending snapshots, so we
   * just need to flip the session mode and transition. */
  const bool can_enter = game->net.welcome_received || game->net.spectating;
  if (can_enter) {
    if (ShroomImGui_Button("Enter Match", 140.0f, 0.0f)) {
      game->selected_mode = SHROOM_SESSION_MODE_LOBBY_PLAY;
      game->active_mode = SHROOM_SESSION_MODE_LOBBY_PLAY;
      ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
    }
  } else {
    ShroomImGui_TextDisabled("Waiting for server...");
  }

  ShroomImGui_End();
}

static void LobbyRosterHandleInput(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  if (game == NULL) {
    return;
  }

  if (!ShroomImGui_WantCaptureKeyboard() && IsKeyPressed(KEY_ESCAPE)) {
    ClientNetSendLobbyLeave(&game->net);
    ClientNetShutdown(&game->net);
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }
}

static void LobbyRosterCleanup(ShroomScreenManager* manager) {
  (void)manager;
  g_ready_state = false;
  g_status_pulse_timer = 0.0f;
}

void ShroomScreenRegisterLobbyRoster(ShroomScreenManager* manager) {
  ShroomScreen* screen;

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
