#include "game.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imgui_wrapper.h"
#include "raymath.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include "shared/sim.h"

static const Color kZoneOuterColor = {36, 48, 44, 255};
static const Color kZoneMidColor = {42, 62, 44, 255};
static const Color kZoneCenterColor = {64, 86, 46, 255};

static const Color kBotColors[] = {
    {255, 173, 96, 255},  {255, 99, 132, 255},  {122, 162, 247, 255},
    {186, 104, 200, 255}, {255, 214, 102, 255},
};

static const float kRemoteInterpolationRate = 10.0f;
static const float kStatusBannerDuration = 2.0f;

static bool IsOnlineMode(GameSessionMode mode) { return mode == SHROOM_SESSION_MODE_QUICK_PLAY; }

typedef enum PlayerThreatState {
  PLAYER_THREAT_NONE = 0,
  PLAYER_THREAT_PREY,
  PLAYER_THREAT_DANGER,
} PlayerThreatState;

typedef struct LeaderboardEntry {
  size_t index;
  float mass;
} LeaderboardEntry;

static ShroomImGuiColor ToImGuiColor(Color color) {
  return (ShroomImGuiColor){(float)color.r / 255.0f, (float)color.g / 255.0f,
                            (float)color.b / 255.0f, (float)color.a / 255.0f};
}

static Color GetLatencyColor(uint32_t rtt_ms) {
  if (rtt_ms >= SHROOM_LATENCY_UNPLAYABLE_MS) {
    return RED;
  }
  if (rtt_ms >= SHROOM_LATENCY_WARNING_MS) {
    return ORANGE;
  }

  return GREEN;
}

static Color GetZoneColor(ShroomZone zone) {
  switch (zone) {
  case SHROOM_ZONE_CENTER:
    return LIME;
  case SHROOM_ZONE_MID:
    return GOLD;
  case SHROOM_ZONE_OUTER:
  default:
    return DARKGREEN;
  }
}

static const char* GetZoneSummary(ShroomZone zone) {
  switch (zone) {
  case SHROOM_ZONE_CENTER:
    return "Highest traffic and biggest swings";
  case SHROOM_ZONE_MID:
    return "Balanced risk and escape routes";
  case SHROOM_ZONE_OUTER:
  default:
    return "Safest respawn and recovery space";
  }
}

static PlayerThreatState GetThreatState(const ShroomPlayerState* local_player,
                                        const ShroomPlayerState* other_player) {
  if ((local_player == NULL) || (other_player == NULL) || (local_player == other_player) ||
      !local_player->alive || !other_player->alive) {
    return PLAYER_THREAT_NONE;
  }

  if (local_player->mass >= (other_player->mass * SHROOM_CONSUME_MASS_ADVANTAGE)) {
    return PLAYER_THREAT_PREY;
  }
  if (other_player->mass >= (local_player->mass * SHROOM_CONSUME_MASS_ADVANTAGE)) {
    return PLAYER_THREAT_DANGER;
  }

  return PLAYER_THREAT_NONE;
}

static Color GetThreatOutlineColor(PlayerThreatState state) {
  switch (state) {
  case PLAYER_THREAT_PREY:
    return SKYBLUE;
  case PLAYER_THREAT_DANGER:
    return RED;
  case PLAYER_THREAT_NONE:
  default:
    return Fade(RAYWHITE, 0.18f);
  }
}

static int GetLocalPlayerRank(const Game* game, const LeaderboardEntry* leaderboard,
                              size_t leaderboard_count) {
  for (size_t index = 0; index < leaderboard_count; ++index) {
    if (&game->world.players[leaderboard[index].index] == game->local_player) {
      return (int)index + 1;
    }
  }

  return 0;
}

static ShroomVec2 ClampPlayerPosition(const ShroomWorldState* world, float radius,
                                      ShroomVec2 position) {
  const float min_x = radius;
  const float min_y = radius;
  const float max_x = world->width - radius;
  const float max_y = world->height - radius;

  if (position.x < min_x) {
    position.x = min_x;
  }
  if (position.x > max_x) {
    position.x = max_x;
  }
  if (position.y < min_y) {
    position.y = min_y;
  }
  if (position.y > max_y) {
    position.y = max_y;
  }

  return position;
}

static void ApplyPredictedInputToPlayer(const ShroomWorldState* world, ShroomPlayerState* player,
                                        ShroomVec2 input_direction, float delta_time) {
  float speed;

  if ((player == NULL) || !player->alive) {
    return;
  }

  speed = ShroomMassToSpeed(player->mass);
  player->input_direction = input_direction;
  player->position = ClampPlayerPosition(
      world, player->radius,
      (ShroomVec2){player->position.x + (input_direction.x * speed * delta_time),
                   player->position.y + (input_direction.y * speed * delta_time)});
}

static void AppendPendingInput(Game* game, uint32_t sequence, ShroomVec2 direction) {
  if (sequence == 0) {
    return;
  }

  if (game->pending_input_count == SHROOM_CLIENT_PENDING_INPUT_CAPACITY) {
    for (uint32_t index = 1; index < game->pending_input_count; ++index) {
      game->pending_inputs[index - 1] = game->pending_inputs[index];
    }
    game->pending_input_count -= 1;
  }

  game->pending_inputs[game->pending_input_count++] =
      (ShroomPendingInput){.sequence = sequence, .direction = direction};
}

static void DiscardAcknowledgedInputs(Game* game) {
  uint32_t keep_index = 0;

  for (uint32_t index = 0; index < game->pending_input_count; ++index) {
    if (game->pending_inputs[index].sequence > game->net.last_processed_input_sequence) {
      game->pending_inputs[keep_index++] = game->pending_inputs[index];
    }
  }

  game->pending_input_count = keep_index;
}

static void ReapplyPendingInputs(Game* game) {
  const float input_delta_time = 1.0f / SHROOM_SERVER_TICK_RATE;

  if ((game->local_player == NULL) || !game->local_player->alive) {
    return;
  }

  for (uint32_t index = 0; index < game->pending_input_count; ++index) {
    ApplyPredictedInputToPlayer(&game->world, game->local_player,
                                game->pending_inputs[index].direction, input_delta_time);
  }
}

static void SyncRenderPositions(Game* game, float delta_time) {
  const float blend = fminf(1.0f, delta_time * kRemoteInterpolationRate);

  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];

    if (!player->alive) {
      game->render_position_initialized[index] = false;
      continue;
    }

    if ((player == game->local_player) || !game->net.welcome_received) {
      game->render_positions[index] = player->position;
      game->render_position_initialized[index] = true;
      continue;
    }

    if (!game->render_position_initialized[index]) {
      game->render_positions[index] = player->position;
      game->render_position_initialized[index] = true;
      continue;
    }

    game->render_positions[index].x +=
        (player->position.x - game->render_positions[index].x) * blend;
    game->render_positions[index].y +=
        (player->position.y - game->render_positions[index].y) * blend;
  }
}

static void ApplyNetworkSnapshot(Game* game) {
  size_t index;

  game->world.tick = game->net.last_snapshot_tick;
  game->world.player_count = game->net.snapshot_player_count;
  game->world.spore_count = game->net.spore_count;

  for (index = 0; index < game->net.snapshot_player_count; ++index) {
    const ShroomSnapshotPlayerState* snapshot_player = &game->net.snapshot_players[index];
    ShroomPlayerState* player = &game->world.players[index];

    *player = (ShroomPlayerState){
        .player_id = snapshot_player->player_id,
        .entity_id = snapshot_player->entity_id,
        .position = {snapshot_player->position_x, snapshot_player->position_y},
        .mass = snapshot_player->mass,
        .radius = snapshot_player->radius,
        .alive = snapshot_player->alive != 0,
        .is_bot = snapshot_player->is_bot != 0,
    };

    if (player->player_id == game->net.player_id) {
      game->local_player = player;
    }
  }

  for (; index < SHROOM_MAX_PLAYERS; ++index) {
    game->render_position_initialized[index] = false;
  }

  for (index = 0; index < game->net.spore_count; ++index) {
    const ShroomSnapshotSporeState* snapshot_spore = &game->net.snapshot_spores[index];
    ShroomSporeState* spore = &game->world.spores[index];

    *spore = (ShroomSporeState){
        .entity_id = snapshot_spore->entity_id,
        .position = {snapshot_spore->position_x, snapshot_spore->position_y},
        .value = snapshot_spore->value,
        .active = true,
    };
  }

  for (; index < SHROOM_MAX_SPORES; ++index) {
    game->world.spores[index].active = false;
  }

  DiscardAcknowledgedInputs(game);
  ReapplyPendingInputs(game);
}

static Color GetPlayerFillColor(const Game* game, const ShroomPlayerState* player) {
  if (game->settings.palette_preset == CLIENT_PALETTE_HIGH_CONTRAST) {
    return player->is_bot ? ORANGE : SKYBLUE;
  }

  if (!player->is_bot) {
    return (Color){126, 217, 87, 255};
  }

  return kBotColors[player->player_id % (sizeof(kBotColors) / sizeof(kBotColors[0]))];
}

static int CompareLeaderboardEntries(const void* left, const void* right) {
  const LeaderboardEntry* lhs = left;
  const LeaderboardEntry* rhs = right;

  if (lhs->mass < rhs->mass) {
    return 1;
  }
  if (lhs->mass > rhs->mass) {
    return -1;
  }

  return 0;
}

static void BuildLeaderboard(const Game* game, LeaderboardEntry* entries, size_t* entry_count) {
  size_t index;

  *entry_count = game->world.player_count;
  for (index = 0; index < game->world.player_count; ++index) {
    entries[index] = (LeaderboardEntry){
        .index = index,
        .mass = game->world.players[index].mass,
    };
  }

  if (*entry_count > 0) {
    qsort(entries, *entry_count, sizeof(entries[0]), CompareLeaderboardEntries);
  }
}

static const char* GetZoneLabel(ShroomZone zone) {
  switch (zone) {
  case SHROOM_ZONE_CENTER:
    return "Center";
  case SHROOM_ZONE_MID:
    return "Mid";
  case SHROOM_ZONE_OUTER:
  default:
    return "Outer";
  }
}

static bool IsConnectionOverlayOpen(const Game* game) {
  if (game->active_mode != SHROOM_SESSION_MODE_QUICK_PLAY) {
    return false;
  }
  return game->net.status != CLIENT_NET_CONNECTED;
}

static bool IsOverlayBlockingGameplay(const Game* game) {
  return game->leaderboard_overlay_open || game->menu_overlay_open ||
         game->leave_confirmation_open || IsConnectionOverlayOpen(game);
}

static void RetryConnection(Game* game) {
  if (game->active_mode != SHROOM_SESSION_MODE_QUICK_PLAY) {
    return;
  }
  ClientNetShutdown(&game->net);
  ClientNetInit(&game->net,
                game->selected_server_host[0] != '\0' ? game->selected_server_host : "127.0.0.1",
                game->selected_server_port != 0 ? game->selected_server_port : SHROOM_SERVER_PORT);
}

static void DrawArenaZones(const ShroomWorldState* world) {
  const Vector2 center = {world->width * 0.5f, world->height * 0.5f};

  DrawRectangle(0, 0, (int)world->width, (int)world->height, Fade(kZoneOuterColor, 0.85f));
  DrawCircleV(center, SHROOM_ZONE_MID_RADIUS, Fade(kZoneMidColor, 0.68f));
  DrawCircleV(center, SHROOM_ZONE_CENTER_RADIUS, Fade(kZoneCenterColor, 0.75f));
  DrawCircleLines((int)center.x, (int)center.y, SHROOM_ZONE_MID_RADIUS, Fade(DARKGREEN, 0.35f));
  DrawCircleLines((int)center.x, (int)center.y, SHROOM_ZONE_CENTER_RADIUS, Fade(LIME, 0.4f));
}

static void DrawSpores(const ShroomWorldState* world) {
  for (size_t index = 0; index < world->spore_count; ++index) {
    const ShroomSporeState* spore = &world->spores[index];

    if (!spore->active) {
      continue;
    }

    DrawCircleV((Vector2){spore->position.x, spore->position.y}, 4.0f, GOLD);
  }
}

static void DrawPlayers(const Game* game) {
  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    const ShroomVec2 render_position = game->render_positions[index];
    const Vector2 position = {render_position.x, render_position.y};
    const Color fill = GetPlayerFillColor(game, player);
    const PlayerThreatState threat_state = GetThreatState(game->local_player, player);
    const Color threat_outline = GetThreatOutlineColor(threat_state);

    if (!player->alive) {
      continue;
    }

    DrawCircleV(position, player->radius, fill);
    DrawCircleLines((int)position.x, (int)position.y, player->radius + 3.0f, Fade(BLACK, 0.55f));
    if (player != game->local_player) {
      DrawCircleLines((int)position.x, (int)position.y, player->radius + 6.0f, threat_outline);
    }

    if (player == game->local_player) {
      DrawCircleLines((int)position.x, (int)position.y, player->radius + 8.0f, RAYWHITE);
    }
  }
}

static void DrawOffscreenIndicators(const Game* game) {
  const Vector2 screen_center = {game->screen_width * 0.5f, game->screen_height * 0.5f};
  const float horizontal_limit = (game->screen_width * 0.5f) - 44.0f;
  const float vertical_limit = (game->screen_height * 0.5f) - 44.0f;

  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    const PlayerThreatState threat_state = GetThreatState(game->local_player, player);
    const Color color = GetThreatOutlineColor(threat_state);
    const Vector2 world_position = {game->render_positions[index].x,
                                    game->render_positions[index].y};
    const Vector2 screen_position = GetWorldToScreen2D(world_position, game->camera);
    Vector2 direction;
    Vector2 indicator_position;
    Vector2 perpendicular;
    Vector2 base_center;
    float scale_x;
    float scale_y;
    float scale;

    if (!player->alive || (threat_state == PLAYER_THREAT_NONE)) {
      continue;
    }
    if ((screen_position.x >= 0.0f) && (screen_position.x <= (float)game->screen_width) &&
        (screen_position.y >= 0.0f) && (screen_position.y <= (float)game->screen_height)) {
      continue;
    }

    direction = Vector2Subtract(screen_position, screen_center);
    if (Vector2LengthSqr(direction) <= 0.01f) {
      continue;
    }

    scale_x = horizontal_limit / fmaxf(fabsf(direction.x), 0.0001f);
    scale_y = vertical_limit / fmaxf(fabsf(direction.y), 0.0001f);
    scale = fminf(scale_x, scale_y);
    indicator_position = Vector2Add(screen_center, Vector2Scale(direction, scale));
    direction = Vector2Normalize(direction);
    perpendicular = (Vector2){-direction.y, direction.x};
    base_center = Vector2Subtract(indicator_position, Vector2Scale(direction, 16.0f));

    DrawLineEx(Vector2Subtract(indicator_position, Vector2Scale(direction, 24.0f)),
               indicator_position, 3.0f, Fade(color, 0.7f));
    DrawTriangle(indicator_position, Vector2Add(base_center, Vector2Scale(perpendicular, 8.0f)),
                 Vector2Subtract(base_center, Vector2Scale(perpendicular, 8.0f)), color);
    DrawCircleV(base_center, 4.0f, Fade(color, 0.75f));
  }
}

static void UpdateStatusBanners(Game* game, float delta_time) {
  const ShroomZone zone = ShroomGetZoneAtPosition(&game->world, game->local_player->position);
  const float moved_distance =
      sqrtf(ShroomDistanceSqr(game->local_player->position, game->previous_local_position));

  if (zone != game->current_zone) {
    game->current_zone = zone;
    game->zone_callout_timer = kStatusBannerDuration;
  } else if (game->zone_callout_timer > 0.0f) {
    game->zone_callout_timer -= delta_time;
  }

  if ((game->previous_local_mass > (SHROOM_DEFAULT_PLAYER_MASS * 1.5f)) &&
      (game->local_player->mass <= (SHROOM_DEFAULT_PLAYER_MASS + 0.1f)) &&
      (moved_distance > 250.0f)) {
    game->respawn_banner_timer = kStatusBannerDuration;
  } else if (game->respawn_banner_timer > 0.0f) {
    game->respawn_banner_timer -= delta_time;
  }

  game->previous_local_mass = game->local_player->mass;
  game->previous_local_position = game->local_player->position;
}

static void DrawStatusBanners(const Game* game) {
  if (game->zone_callout_timer > 0.0f) {
    ShroomImGui_SetNextWindowPos((game->screen_width - 420) * 0.5f, 18.0f,
                                 SHROOM_IMGUI_COND_ALWAYS);
    ShroomImGui_SetNextWindowSize(420.0f, 76.0f, SHROOM_IMGUI_COND_ALWAYS);
    ShroomImGui_SetNextWindowBgAlpha(0.74f);
    if (ShroomImGui_Begin("Zone Banner", NULL,
                          SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                              SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                              SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS |
                              SHROOM_IMGUI_WINDOW_NO_SCROLLBAR)) {
      ShroomImGui_TextColored(ToImGuiColor(GetZoneColor(game->current_zone)),
                              TextFormat("%s Zone", GetZoneLabel(game->current_zone)));
      ShroomImGui_Text(GetZoneSummary(game->current_zone));
    }
    ShroomImGui_End();
  }

  if (game->respawn_banner_timer > 0.0f) {
    ShroomImGui_SetNextWindowPos((game->screen_width - 360) * 0.5f, 98.0f,
                                 SHROOM_IMGUI_COND_ALWAYS);
    ShroomImGui_SetNextWindowSize(360.0f, 56.0f, SHROOM_IMGUI_COND_ALWAYS);
    ShroomImGui_SetNextWindowBgAlpha(0.78f);
    if (ShroomImGui_Begin("Respawn Banner", NULL,
                          SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                              SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                              SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS |
                              SHROOM_IMGUI_WINDOW_NO_SCROLLBAR)) {
      ShroomImGui_TextColored(ToImGuiColor(ORANGE), "Consumed - respawned in the outer ring");
    }
    ShroomImGui_End();
  }
}

static void DrawLeaderboardOverlay(Game* game, const LeaderboardEntry* leaderboard,
                                   size_t shown_count) {
  if (!game->leaderboard_overlay_open) {
    return;
  }

  ShroomImGui_SetNextWindowPos((game->screen_width - 440) * 0.5f, 100.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(440.0f, 280.0f, SHROOM_IMGUI_COND_ALWAYS);
  if (!ShroomImGui_Begin("Leaderboard", NULL,
                         SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                             SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_Text("Tab or Enter closes the leaderboard.");
  ShroomImGui_Separator();

  for (size_t index = 0; index < shown_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[leaderboard[index].index];
    const PlayerThreatState threat_state = GetThreatState(game->local_player, player);
    const char* label = player == game->local_player ? "You" : (player->is_bot ? "Bot" : "Player");
    const Color color =
        player == game->local_player ? RAYWHITE : GetThreatOutlineColor(threat_state);

    ShroomImGui_TextColored(ToImGuiColor(color),
                            TextFormat("%d. %s %u   %.0f mass", (int)(index + 1), label,
                                       player->player_id, player->mass));
  }

  if (ShroomImGui_Button("Close", 120.0f, 0.0f)) {
    game->leaderboard_overlay_open = false;
  }

  ShroomImGui_End();
}

static void DrawMenuOverlay(Game* game) {
  if (!game->menu_overlay_open) {
    return;
  }

  ShroomImGui_SetNextWindowPos((game->screen_width - 420) * 0.5f,
                               (game->screen_height - 280) * 0.5f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(420.0f, 280.0f, SHROOM_IMGUI_COND_ALWAYS);
  if (!ShroomImGui_Begin("Match Menu", NULL,
                         SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                             SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_Text(TextFormat("Server: %s", ClientNetStatusLabel(&game->net)));
  ShroomImGui_TextColored(
      ToImGuiColor(GetLatencyColor(game->net.rtt_average_ms)),
      TextFormat("Ping: %ums  Avg: %ums", game->net.rtt_ms, game->net.rtt_average_ms));
  ShroomImGui_Text("Esc / Enter resumes, Tab toggles leaderboard.");
  ShroomImGui_Separator();

  if (ShroomImGui_Button("Resume", -1.0f, 0.0f)) {
    game->menu_overlay_open = false;
  }
  if (ShroomImGui_Button("Show Leaderboard", -1.0f, 0.0f)) {
    game->leaderboard_overlay_open = true;
    game->menu_overlay_open = false;
  }
  if (ShroomImGui_Button("Retry Connection", -1.0f, 0.0f)) {
    RetryConnection(game);
  }
  if (ShroomImGui_Button("Return To Main Menu", -1.0f, 0.0f)) {
    game->leave_confirmation_open = true;
    game->menu_overlay_open = false;
  }

  ShroomImGui_End();
}

static void DrawLeaveConfirmationOverlay(Game* game) {
  if (!game->leave_confirmation_open) {
    return;
  }

  ShroomImGui_SetNextWindowPos((game->screen_width - 340) * 0.5f,
                               (game->screen_height - 170) * 0.5f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(340.0f, 170.0f, SHROOM_IMGUI_COND_ALWAYS);
  if (!ShroomImGui_Begin("Leave Match?", NULL,
                         SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                             SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_TextWrapped("Leave the current match and return to the main menu?");
  ShroomImGui_Spacing();

  if (ShroomImGui_Button("Leave Match", 140.0f, 0.0f)) {
    game->return_to_menu_requested = true;
    game->leave_confirmation_open = false;
    game->leaderboard_overlay_open = false;
    game->menu_overlay_open = false;
  }
  ShroomImGui_SameLine();
  if (ShroomImGui_Button("Stay", 140.0f, 0.0f)) {
    game->leave_confirmation_open = false;
    game->menu_overlay_open = true;
  }

  ShroomImGui_End();
}

static void DrawConnectionOverlay(Game* game) {
  if (!IsConnectionOverlayOpen(game)) {
    return;
  }

  ShroomImGui_SetNextWindowPos(22.0f, game->screen_height - 160.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(320.0f, 138.0f, SHROOM_IMGUI_COND_ALWAYS);
  if (!ShroomImGui_Begin("Connection", NULL,
                         SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                             SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_TextColored(ToImGuiColor(GetLatencyColor(game->net.rtt_average_ms)),
                          TextFormat("Status: %s", ClientNetStatusLabel(&game->net)));
  ShroomImGui_Text(TextFormat("Target: %s:%u", game->selected_server_host,
                              (unsigned int)game->selected_server_port));
  ShroomImGui_Text("R retries connection. B returns to the main menu.");

  if (ShroomImGui_Button("Retry", 120.0f, 0.0f)) {
    RetryConnection(game);
  }
  ShroomImGui_SameLine();
  if (ShroomImGui_Button("Back To Menu", 140.0f, 0.0f)) {
    game->return_to_menu_requested = true;
  }

  ShroomImGui_End();
}

static void DrawDiagnosticsOverlay(const Game* game) {
  if (!game->diagnostics_overlay_open) {
    return;
  }

  ShroomImGui_SetNextWindowPos(game->screen_width - 330.0f, game->screen_height - 210.0f,
                               SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(308.0f, 188.0f, SHROOM_IMGUI_COND_ALWAYS);
  if (!ShroomImGui_Begin("Diagnostics", NULL,
                         SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                             SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_Text(
      TextFormat("Snapshot Tick: %llu", (unsigned long long)game->net.last_snapshot_tick));
  ShroomImGui_Text(TextFormat("Input Sequence: %u", game->net.last_input_sequence));
  ShroomImGui_Text(TextFormat("Pending Inputs: %u", game->pending_input_count));
  ShroomImGui_Text(TextFormat("Players: %u  Spores: %u", (unsigned int)game->world.player_count,
                              (unsigned int)game->world.spore_count));
  ShroomImGui_Text(TextFormat("Local Mass: %.1f", game->local_player->mass));
  ShroomImGui_Text(TextFormat("RTT: %ums  Avg: %ums", game->net.rtt_ms, game->net.rtt_average_ms));

  ShroomImGui_End();
}

static void DrawGameplayHud(const Game* game, int local_rank, size_t leaderboard_count,
                            ShroomZone zone) {
  ShroomImGui_SetNextWindowPos(18.0f, 18.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(316.0f, 124.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.66f);
  if (ShroomImGui_Begin("HUD Left", NULL,
                        SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                            SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                            SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS |
                            SHROOM_IMGUI_WINDOW_NO_SCROLLBAR)) {
    ShroomImGui_Text("shroomio");
    ShroomImGui_Text(TextFormat("Mass %.0f", game->local_player->mass));
    ShroomImGui_Text(TextFormat("Rank %d/%d", local_rank > 0 ? local_rank : (int)leaderboard_count,
                                (int)leaderboard_count));
    ShroomImGui_TextColored(ToImGuiColor(GetZoneColor(zone)),
                            TextFormat("Zone %s", GetZoneLabel(zone)));
    ShroomImGui_Text(TextFormat("Players %d   Spores %d", (int)game->world.player_count,
                                (int)game->world.spore_count));
  }
  ShroomImGui_End();

  ShroomImGui_SetNextWindowPos(game->screen_width - 232.0f, 18.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(214.0f, 106.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.66f);
  if (ShroomImGui_Begin("HUD Right", NULL,
                        SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                            SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                            SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS |
                            SHROOM_IMGUI_WINDOW_NO_SCROLLBAR)) {
    if (game->settings.show_ping_ms) {
      ShroomImGui_TextColored(ToImGuiColor(GetLatencyColor(game->net.rtt_average_ms)),
                              TextFormat("Ping %ums", game->net.rtt_average_ms));
    }
    ShroomImGui_Text(TextFormat("Server %s", ClientNetStatusLabel(&game->net)));
    ShroomImGui_Text("Tab Leaderboard");
    ShroomImGui_Text("Esc Match Menu");
    ShroomImGui_Text("F3 Diagnostics");
  }
  ShroomImGui_End();
}

static ShroomVec2 GetMovementInput(const Game* game) {
  const Vector2 mouse_screen = GetMousePosition();
  const Vector2 mouse_world = GetScreenToWorld2D(mouse_screen, game->camera);
  const Vector2 player_world = {game->local_player->position.x, game->local_player->position.y};
  Vector2 movement = Vector2Subtract(mouse_world, player_world);

  if (game->settings.invert_mouse) {
    movement = Vector2Scale(movement, -1.0f);
  }

  if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
    movement.x += 1.0f;
  }
  if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
    movement.x -= 1.0f;
  }
  if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
    movement.y -= 1.0f;
  }
  if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
    movement.y += 1.0f;
  }

  if (Vector2LengthSqr(movement) <= 16.0f) {
    return (ShroomVec2){0};
  }

  movement = Vector2Normalize(movement);
  return (ShroomVec2){movement.x, movement.y};
}

void GameInit(Game* game, int screen_width, int screen_height, GameSessionMode mode) {
  size_t bot_index;
  ClientSettings settings = game->settings;
  GameSessionMode selected_mode = game->selected_mode;
  char selected_server_host[sizeof(game->selected_server_host)] = {0};
  uint16_t selected_server_port = game->selected_server_port;

  snprintf(selected_server_host, sizeof(selected_server_host), "%s",
           game->selected_server_host[0] != '\0' ? game->selected_server_host : "127.0.0.1");
  if (selected_server_port == 0) {
    selected_server_port = SHROOM_SERVER_PORT;
  }

  *game = (Game){0};
  game->settings = settings;
  game->selected_mode = selected_mode;
  game->active_mode = mode;
  snprintf(game->selected_server_host, sizeof(game->selected_server_host), "%s",
           selected_server_host);
  game->selected_server_port = selected_server_port;
  game->screen_width = screen_width;
  game->screen_height = screen_height;

  if (IsOnlineMode(mode)) {
    ClientNetInit(&game->net, game->selected_server_host, game->selected_server_port);
  } else {
    game->net.status = CLIENT_NET_CONNECTED;
    snprintf(game->net.status_text, sizeof(game->net.status_text), "%s", "Offline");
  }
  ShroomWorldInit(&game->world);
  game->local_player = ShroomWorldSpawnPlayer(&game->world, 1, false);
  for (bot_index = 0; bot_index < SHROOM_BOT_COUNT; ++bot_index) {
    ShroomWorldSpawnPlayer(&game->world, (ShroomPlayerId)(bot_index + 2), true);
  }

  game->camera.offset = (Vector2){screen_width / 2.0f, screen_height / 2.0f};
  game->camera.target = (Vector2){game->local_player->position.x, game->local_player->position.y};
  game->camera.rotation = 0.0f;
  game->camera.zoom = 1.0f;
  game->current_zone = ShroomGetZoneAtPosition(&game->world, game->local_player->position);
  game->previous_local_position = game->local_player->position;
  game->previous_local_mass = game->local_player->mass;
  game->zone_callout_timer = kStatusBannerDuration;
  game->diagnostics_overlay_open = game->settings.diagnostics_enabled;
}

void GameUpdate(Game* game, float delta_time) {
  const ShroomVec2 input_direction =
      IsOverlayBlockingGameplay(game) ? (ShroomVec2){0} : GetMovementInput(game);
  const uint32_t previous_input_sequence = game->net.last_input_sequence;

  if (IsOnlineMode(game->active_mode)) {
    ClientNetUpdate(&game->net, input_direction, delta_time);
  }

  if (IsOnlineMode(game->active_mode) &&
      (game->net.last_input_sequence > previous_input_sequence)) {
    for (uint32_t sequence = previous_input_sequence + 1; sequence <= game->net.last_input_sequence;
         ++sequence) {
      AppendPendingInput(game, sequence, input_direction);
    }
    game->tracked_input_sequence = game->net.last_input_sequence;
  }

  if (IsOnlineMode(game->active_mode) && game->net.welcome_received &&
      (game->net.snapshot_player_count > 0)) {
    ApplyNetworkSnapshot(game);
    if (game->local_player != NULL) {
      ApplyPredictedInputToPlayer(&game->world, game->local_player, input_direction, delta_time);
    }
  } else {
    ShroomPlayerSetInput(game->local_player, input_direction);
    ShroomWorldStep(&game->world, delta_time);
  }

  SyncRenderPositions(game, delta_time);
  UpdateStatusBanners(game, delta_time);
  game->camera.target = (Vector2){game->local_player->position.x, game->local_player->position.y};
}

void GameDraw(Game* game) {
  LeaderboardEntry leaderboard[SHROOM_MAX_PLAYERS];
  size_t leaderboard_count = 0;
  size_t shown_count;
  const ShroomZone zone = ShroomGetZoneAtPosition(&game->world, game->local_player->position);
  int local_rank;

  BuildLeaderboard(game, leaderboard, &leaderboard_count);
  local_rank = GetLocalPlayerRank(game, leaderboard, leaderboard_count);
  shown_count = leaderboard_count < 8 ? leaderboard_count : 8;

  ClearBackground((Color){18, 18, 26, 255});

  BeginMode2D(game->camera);
  DrawArenaZones(&game->world);
  DrawRectangleLines(0, 0, (int)game->world.width, (int)game->world.height, Fade(DARKGREEN, 0.7f));
  DrawGrid(80, 64.0f);
  DrawSpores(&game->world);
  DrawPlayers(game);
  EndMode2D();

  DrawOffscreenIndicators(game);
  DrawGameplayHud(game, local_rank, leaderboard_count, zone);
  DrawStatusBanners(game);
  DrawLeaderboardOverlay(game, leaderboard, shown_count);
  DrawMenuOverlay(game);
  DrawLeaveConfirmationOverlay(game);
  DrawConnectionOverlay(game);
  DrawDiagnosticsOverlay(game);
}

void GameShutdown(Game* game) {
  if (IsOnlineMode(game->active_mode)) {
    ClientNetShutdown(&game->net);
  }
}
