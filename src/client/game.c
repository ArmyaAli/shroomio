#include "game.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "raymath.h"

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

typedef enum PlayerThreatState {
  PLAYER_THREAT_NONE = 0,
  PLAYER_THREAT_PREY,
  PLAYER_THREAT_DANGER,
} PlayerThreatState;

typedef struct LeaderboardEntry {
  size_t index;
  float mass;
} LeaderboardEntry;

static bool IsOnlineMode(GameSessionMode mode) { return mode == SHROOM_SESSION_MODE_QUICK_PLAY; }

static bool UseHighContrastPalette(const Game* game) {
  return (game != NULL) && (game->settings.palette_preset == CLIENT_PALETTE_HIGH_CONTRAST);
}

static const char* GetSessionModeLabel(GameSessionMode mode) {
  switch (mode) {
  case SHROOM_SESSION_MODE_OFFLINE_PRACTICE:
    return "Offline Practice";
  case SHROOM_SESSION_MODE_QUICK_PLAY:
  default:
    return "Quick Play";
  }
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

static Color GetZoneColor(const Game* game, ShroomZone zone) {
  if (UseHighContrastPalette(game)) {
    switch (zone) {
    case SHROOM_ZONE_CENTER:
      return ORANGE;
    case SHROOM_ZONE_MID:
      return SKYBLUE;
    case SHROOM_ZONE_OUTER:
    default:
      return YELLOW;
    }
  }

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

static Color GetThreatOutlineColor(const Game* game, PlayerThreatState state) {
  if (UseHighContrastPalette(game)) {
    switch (state) {
    case PLAYER_THREAT_PREY:
      return YELLOW;
    case PLAYER_THREAT_DANGER:
      return MAGENTA;
    case PLAYER_THREAT_NONE:
    default:
      return Fade(RAYWHITE, 0.34f);
    }
  }

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

static Color GetPlayerFillColor(const ShroomPlayerState* player) {
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

static void DrawArenaZones(const Game* game) {
  const ShroomWorldState* world = &game->world;
  const Vector2 center = {world->width * 0.5f, world->height * 0.5f};
  const Color outer_color =
      UseHighContrastPalette(game) ? (Color){34, 34, 24, 255} : kZoneOuterColor;
  const Color mid_color = UseHighContrastPalette(game) ? (Color){22, 60, 82, 255} : kZoneMidColor;
  const Color center_color =
      UseHighContrastPalette(game) ? (Color){104, 46, 0, 255} : kZoneCenterColor;
  const Color mid_outline = UseHighContrastPalette(game) ? SKYBLUE : DARKGREEN;
  const Color center_outline = UseHighContrastPalette(game) ? ORANGE : LIME;

  DrawRectangle(0, 0, (int)world->width, (int)world->height, Fade(outer_color, 0.85f));
  DrawCircleV(center, SHROOM_ZONE_MID_RADIUS, Fade(mid_color, 0.68f));
  DrawCircleV(center, SHROOM_ZONE_CENTER_RADIUS, Fade(center_color, 0.75f));
  DrawCircleLines((int)center.x, (int)center.y, SHROOM_ZONE_MID_RADIUS, Fade(mid_outline, 0.35f));
  DrawCircleLines((int)center.x, (int)center.y, SHROOM_ZONE_CENTER_RADIUS,
                  Fade(center_outline, 0.4f));
}

static void DrawSpores(const ShroomWorldState* world) {
  size_t index;

  for (index = 0; index < world->spore_count; ++index) {
    const ShroomSporeState* spore = &world->spores[index];

    if (!spore->active) {
      continue;
    }

    DrawCircleV((Vector2){spore->position.x, spore->position.y}, 4.0f, GOLD);
  }
}

static void DrawPlayers(const Game* game) {
  size_t index;

  for (index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    const ShroomVec2 render_position = game->render_positions[index];
    const Vector2 position = {render_position.x, render_position.y};
    const Color fill = GetPlayerFillColor(player);
    const PlayerThreatState threat_state = GetThreatState(game->local_player, player);
    const Color threat_outline = GetThreatOutlineColor(game, threat_state);

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
    const Color color = GetThreatOutlineColor(game, threat_state);
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

static void DrawZoneLegend(const Game* game) {
  DrawRectangle(24, game->screen_height - 108, 330, 84, Fade(BLACK, 0.42f));
  DrawText("Zone Guide", 40, game->screen_height - 100, 20, RAYWHITE);
  DrawText("Outer: safest recovery", 40, game->screen_height - 74, 18,
           GetZoneColor(game, SHROOM_ZONE_OUTER));
  DrawText("Mid: balanced pressure", 40, game->screen_height - 52, 18,
           GetZoneColor(game, SHROOM_ZONE_MID));
  DrawText("Center: highest contest", 40, game->screen_height - 30, 18,
           GetZoneColor(game, SHROOM_ZONE_CENTER));
}

static void DrawStatusBanners(const Game* game) {
  if (game->zone_callout_timer > 0.0f) {
    DrawRectangle(game->screen_width / 2 - 220, 24, 440, 66, Fade(BLACK, 0.48f));
    DrawText(TextFormat("%s Zone", GetZoneLabel(game->current_zone)), game->screen_width / 2 - 86,
             34, 26, GetZoneColor(game, game->current_zone));
    DrawText(GetZoneSummary(game->current_zone), game->screen_width / 2 - 156, 62, 18, RAYWHITE);
  }

  if (game->respawn_banner_timer > 0.0f) {
    DrawRectangle(game->screen_width / 2 - 190, 98, 380, 52, Fade(BLACK, 0.52f));
    DrawText("Consumed - respawned in the outer ring", game->screen_width / 2 - 156, 114, 20,
             ORANGE);
  }
}

static void DrawLeaderboardOverlay(const Game* game, const LeaderboardEntry* leaderboard,
                                   size_t shown_count) {
  size_t index;

  if (!game->leaderboard_overlay_open) {
    return;
  }

  DrawRectangle(0, 0, game->screen_width, game->screen_height, Fade(BLACK, 0.45f));
  DrawRectangle(game->screen_width / 2 - 220, 110, 440, 236, Fade((Color){16, 20, 28, 255}, 0.96f));
  DrawRectangleLines(game->screen_width / 2 - 220, 110, 440, 236, Fade(RAYWHITE, 0.18f));
  DrawText("Leaderboard", game->screen_width / 2 - 70, 128, 28, RAYWHITE);
  DrawText("Press Tab to close", game->screen_width / 2 - 78, 158, 18, GRAY);

  for (index = 0; index < shown_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[leaderboard[index].index];
    const PlayerThreatState threat_state = GetThreatState(game->local_player, player);
    const char* label = player == game->local_player ? "You" : (player->is_bot ? "Bot" : "Player");
    const Color color =
        player == game->local_player ? RAYWHITE : GetThreatOutlineColor(game, threat_state);

    DrawText(TextFormat("%d.", (int)(index + 1)), game->screen_width / 2 - 182,
             198 + ((int)index * 22), 20, color);
    DrawText(TextFormat("%s %u", label, player->player_id), game->screen_width / 2 - 146,
             198 + ((int)index * 22), 20, color);
    DrawText(TextFormat("%.0f", player->mass), game->screen_width / 2 + 140,
             198 + ((int)index * 22), 20, color);
  }
}

static void DrawMenuOverlay(const Game* game) {
  if (!game->menu_overlay_open) {
    return;
  }

  DrawRectangle(0, 0, game->screen_width, game->screen_height, Fade(BLACK, 0.5f));
  DrawRectangle(game->screen_width / 2 - 220, game->screen_height / 2 - 140, 440, 280,
                Fade((Color){14, 18, 24, 255}, 0.97f));
  DrawRectangleLines(game->screen_width / 2 - 220, game->screen_height / 2 - 140, 440, 280,
                     Fade(RAYWHITE, 0.18f));

  if (game->active_mode == SHROOM_SESSION_MODE_OFFLINE_PRACTICE) {
    DrawText("Offline Pause", game->screen_width / 2 - 92, game->screen_height / 2 - 112, 30,
             RAYWHITE);
    DrawText("Esc / Enter: Resume", game->screen_width / 2 - 136, game->screen_height / 2 - 58, 20,
             GRAY);
    DrawText("M: Return to main menu", game->screen_width / 2 - 136, game->screen_height / 2 - 30,
             20, GRAY);
    DrawText("Simulation pauses while this menu is open.", game->screen_width / 2 - 136,
             game->screen_height / 2 + 28, 20, RAYWHITE);
    return;
  }

  DrawText("Online Match", game->screen_width / 2 - 90, game->screen_height / 2 - 112, 30,
           RAYWHITE);
  DrawText("Esc / Enter: Resume", game->screen_width / 2 - 136, game->screen_height / 2 - 58, 20,
           GRAY);
  DrawText("L or M: Leave match", game->screen_width / 2 - 136, game->screen_height / 2 - 30, 20,
           GRAY);
  DrawText("Current Status", game->screen_width / 2 - 136, game->screen_height / 2 + 24, 22,
           RAYWHITE);
  DrawText(TextFormat("Server: %s", ClientNetStatusLabel(&game->net)), game->screen_width / 2 - 136,
           game->screen_height / 2 + 58, 20,
           game->net.status == CLIENT_NET_CONNECTED ? GREEN : ORANGE);
  DrawText(TextFormat("Ping: %ums  Avg: %ums", game->net.rtt_ms, game->net.rtt_average_ms),
           game->screen_width / 2 - 136, game->screen_height / 2 + 86, 20,
           GetLatencyColor(game->net.rtt_average_ms));
}

static void DrawLeaveConfirmationOverlay(const Game* game) {
  if (!game->leave_confirmation_open) {
    return;
  }

  DrawRectangle(0, 0, game->screen_width, game->screen_height, Fade(BLACK, 0.58f));
  DrawRectangle(game->screen_width / 2 - 220, game->screen_height / 2 - 104, 440, 208,
                Fade((Color){18, 20, 28, 255}, 0.98f));
  DrawRectangleLines(game->screen_width / 2 - 220, game->screen_height / 2 - 104, 440, 208,
                     Fade(RAYWHITE, 0.18f));
  DrawText("Leave Match?", game->screen_width / 2 - 86, game->screen_height / 2 - 72, 30, RAYWHITE);
  DrawText("This closes the online session and returns to the main menu.",
           game->screen_width / 2 - 184, game->screen_height / 2 - 20, 20, GRAY);
  DrawText("Enter / Y: Leave", game->screen_width / 2 - 96, game->screen_height / 2 + 28, 20,
           ORANGE);
  DrawText("Esc / N: Stay", game->screen_width / 2 - 96, game->screen_height / 2 + 58, 20,
           RAYWHITE);
}

static void DrawConnectionOverlay(const Game* game) {
  if (!IsOnlineMode(game->active_mode) || game->net.welcome_received || game->menu_overlay_open ||
      game->leave_confirmation_open) {
    return;
  }

  DrawRectangle(0, 0, game->screen_width, game->screen_height, Fade(BLACK, 0.48f));
  DrawRectangle(game->screen_width / 2 - 240, game->screen_height / 2 - 112, 480, 224,
                Fade((Color){16, 20, 28, 255}, 0.97f));
  DrawRectangleLines(game->screen_width / 2 - 240, game->screen_height / 2 - 112, 480, 224,
                     Fade(RAYWHITE, 0.18f));
  DrawText("Quick Play", game->screen_width / 2 - 70, game->screen_height / 2 - 78, 30, RAYWHITE);
  DrawText(TextFormat("Status: %s", ClientNetStatusLabel(&game->net)), game->screen_width / 2 - 110,
           game->screen_height / 2 - 26, 24, game->net.status == CLIENT_NET_ERROR ? RED : ORANGE);

  if ((game->net.status == CLIENT_NET_ERROR) || (game->net.status == CLIENT_NET_DISCONNECTED)) {
    DrawText("R: Retry connection", game->screen_width / 2 - 110, game->screen_height / 2 + 24, 20,
             RAYWHITE);
    DrawText("Esc / B: Back to menu", game->screen_width / 2 - 110, game->screen_height / 2 + 52,
             20, GRAY);
  } else {
    DrawText("Esc: Cancel and return to menu", game->screen_width / 2 - 138,
             game->screen_height / 2 + 38, 20, GRAY);
  }
}

static void DrawGameplayHud(const Game* game, int local_rank, size_t leaderboard_count,
                            ShroomZone zone) {
  DrawRectangle(24, 24, 344, 132, Fade(BLACK, 0.38f));
  DrawRectangle(game->screen_width - 236, 24, 196, 116, Fade(BLACK, 0.38f));

  DrawText("shroomio", 40, 32, 34, RAYWHITE);
  DrawText(TextFormat("Mass %.0f", game->local_player->mass), 40, 76, 28, RAYWHITE);
  DrawText(TextFormat("Rank %d/%d", local_rank > 0 ? local_rank : (int)leaderboard_count,
                      (int)leaderboard_count),
           40, 110, 20, GRAY);
  DrawText(TextFormat("Zone %s", GetZoneLabel(zone)), 188, 110, 20, GetZoneColor(game, zone));
  DrawText(TextFormat("Players %d   Spores %d", (int)game->world.player_count,
                      (int)game->world.spore_count),
           40, 136, 18, GRAY);
  DrawText(GetSessionModeLabel(game->active_mode), 188, 136, 18,
           IsOnlineMode(game->active_mode) ? SKYBLUE : LIME);

  if (IsOnlineMode(game->active_mode)) {
    DrawText(TextFormat("Ping %ums", game->net.rtt_average_ms), game->screen_width - 216, 40, 24,
             GetLatencyColor(game->net.rtt_average_ms));
    DrawText(TextFormat("Server %s", ClientNetStatusLabel(&game->net)), game->screen_width - 216,
             72, 18, game->net.status == CLIENT_NET_CONNECTED ? GREEN : ORANGE);
    DrawText(TextFormat("Snapshot %llu", game->net.last_snapshot_tick), game->screen_width - 216,
             98, 18, GRAY);
  } else {
    DrawText("Offline session", game->screen_width - 216, 40, 24, LIME);
    DrawText("Esc pauses local simulation", game->screen_width - 216, 74, 18, GRAY);
  }

  DrawText("Tab Leaderboard", game->screen_width - 216, 118, 18, GRAY);
}

static ShroomVec2 GetMovementInput(const Game* game) {
  const Vector2 mouse_screen = GetMousePosition();
  const Vector2 mouse_world = GetScreenToWorld2D(mouse_screen, game->camera);
  const Vector2 player_world = {
      game->local_player->position.x,
      game->local_player->position.y,
  };
  Vector2 movement = Vector2Subtract(mouse_world, player_world);

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
  ClientSettings settings = game->settings;
  char selected_server_host[sizeof(game->selected_server_host)] = {0};
  uint16_t selected_server_port = game->selected_server_port;

  if (game->selected_server_host[0] != '\0') {
    snprintf(selected_server_host, sizeof(selected_server_host), "%s", game->selected_server_host);
  }

  *game = (Game){0};

  game->settings = settings;
  game->selected_mode = mode;
  game->active_mode = mode;
  game->selected_server_port = selected_server_port;
  if (selected_server_host[0] != '\0') {
    snprintf(game->selected_server_host, sizeof(game->selected_server_host), "%s",
             selected_server_host);
  }
  game->screen_width = screen_width;
  game->screen_height = screen_height;

  ShroomWorldInit(&game->world);
  game->local_player = ShroomWorldSpawnPlayer(&game->world, 1, false);

  if (IsOnlineMode(mode)) {
    const char* host_name =
        game->selected_server_host[0] != '\0' ? game->selected_server_host : "127.0.0.1";
    const uint16_t port =
        game->selected_server_port != 0 ? game->selected_server_port : SHROOM_SERVER_PORT;
    ClientNetInit(&game->net, host_name, port);
  } else {
    size_t bot_index;

    for (bot_index = 0; bot_index < SHROOM_BOT_COUNT; ++bot_index) {
      ShroomWorldSpawnPlayer(&game->world, (ShroomPlayerId)(bot_index + 2), true);
    }
  }

  game->camera.offset = (Vector2){screen_width / 2.0f, screen_height / 2.0f};
  game->camera.target = (Vector2){game->local_player->position.x, game->local_player->position.y};
  game->camera.rotation = 0.0f;
  game->camera.zoom = 1.0f;
  game->current_zone = ShroomGetZoneAtPosition(&game->world, game->local_player->position);
  game->previous_local_position = game->local_player->position;
  game->previous_local_mass = game->local_player->mass;
  game->zone_callout_timer = kStatusBannerDuration;
}

void GameUpdate(Game* game, float delta_time) {
  const bool overlay_open =
      game->leaderboard_overlay_open || game->menu_overlay_open || game->leave_confirmation_open;
  const bool offline_paused = (game->active_mode == SHROOM_SESSION_MODE_OFFLINE_PRACTICE) &&
                              (game->menu_overlay_open || game->leave_confirmation_open);
  ShroomVec2 input_direction;

  if (overlay_open) {
    input_direction = (ShroomVec2){0};
  } else {
    input_direction = GetMovementInput(game);
  }

  if (IsOnlineMode(game->active_mode)) {
    const uint32_t previous_input_sequence = game->net.last_input_sequence;

    ClientNetUpdate(&game->net, input_direction, delta_time);

    if (game->net.last_input_sequence > previous_input_sequence) {
      for (uint32_t sequence = previous_input_sequence + 1;
           sequence <= game->net.last_input_sequence; ++sequence) {
        AppendPendingInput(game, sequence, input_direction);
      }
      game->tracked_input_sequence = game->net.last_input_sequence;
    }

    if (game->net.welcome_received && (game->net.snapshot_player_count > 0)) {
      ApplyNetworkSnapshot(game);
      if (game->local_player != NULL) {
        ApplyPredictedInputToPlayer(&game->world, game->local_player, input_direction, delta_time);
      }
    }
  } else {
    ShroomPlayerSetInput(game->local_player, input_direction);
    if (!offline_paused) {
      ShroomWorldStep(&game->world, delta_time);
    }
  }

  SyncRenderPositions(game, delta_time);
  UpdateStatusBanners(game, delta_time);

  game->camera.target = (Vector2){game->local_player->position.x, game->local_player->position.y};
}

void GameDraw(const Game* game) {
  LeaderboardEntry leaderboard[SHROOM_MAX_PLAYERS];
  size_t leaderboard_count = 0;
  size_t shown_count;
  const ShroomZone zone = ShroomGetZoneAtPosition(&game->world, game->local_player->position);
  int local_rank;

  BuildLeaderboard(game, leaderboard, &leaderboard_count);
  local_rank = GetLocalPlayerRank(game, leaderboard, leaderboard_count);
  shown_count = leaderboard_count < 6 ? leaderboard_count : 6;

  BeginDrawing();
  ClearBackground((Color){18, 18, 26, 255});

  BeginMode2D(game->camera);

  DrawArenaZones(game);
  DrawRectangleLines(0, 0, (int)game->world.width, (int)game->world.height, Fade(DARKGREEN, 0.7f));
  DrawGrid(80, 64.0f);
  DrawSpores(&game->world);
  DrawPlayers(game);

  EndMode2D();

  DrawOffscreenIndicators(game);
  DrawGameplayHud(game, local_rank, leaderboard_count, zone);
  DrawZoneLegend(game);
  DrawStatusBanners(game);
  DrawLeaderboardOverlay(game, leaderboard, shown_count);
  DrawMenuOverlay(game);
  DrawLeaveConfirmationOverlay(game);
  DrawConnectionOverlay(game);

  EndDrawing();
}

void GameShutdown(Game* game) { ClientNetShutdown(&game->net); }
