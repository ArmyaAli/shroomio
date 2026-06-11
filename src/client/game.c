#include "game.h"

#include <stddef.h>
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

typedef struct LeaderboardEntry {
  size_t index;
  float mass;
} LeaderboardEntry;

static void ApplyNetworkSnapshot(Game* game) {
  size_t index;

  game->world.tick = game->net.last_snapshot_tick;
  game->world.player_count = game->net.snapshot_player_count;
  game->world.spore_count = 0;

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

static void DrawArenaZones(const ShroomWorldState* world) {
  const Vector2 center = {world->width * 0.5f, world->height * 0.5f};

  DrawRectangle(0, 0, (int)world->width, (int)world->height, Fade(kZoneOuterColor, 0.85f));
  DrawCircleV(center, SHROOM_ZONE_MID_RADIUS, Fade(kZoneMidColor, 0.68f));
  DrawCircleV(center, SHROOM_ZONE_CENTER_RADIUS, Fade(kZoneCenterColor, 0.75f));
  DrawCircleLines((int)center.x, (int)center.y, SHROOM_ZONE_MID_RADIUS, Fade(DARKGREEN, 0.35f));
  DrawCircleLines((int)center.x, (int)center.y, SHROOM_ZONE_CENTER_RADIUS, Fade(LIME, 0.4f));
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
    const Vector2 position = {player->position.x, player->position.y};
    const Color fill = GetPlayerFillColor(player);

    if (!player->alive) {
      continue;
    }

    DrawCircleV(position, player->radius, fill);
    DrawCircleLines((int)position.x, (int)position.y, player->radius + 3.0f, Fade(BLACK, 0.55f));

    if (player == game->local_player) {
      DrawCircleLines((int)position.x, (int)position.y, player->radius + 8.0f, RAYWHITE);
    }
  }
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

void GameInit(Game* game, int screen_width, int screen_height) {
  size_t bot_index;

  game->screen_width = screen_width;
  game->screen_height = screen_height;

  ClientNetInit(&game->net, "127.0.0.1", SHROOM_SERVER_PORT);
  ShroomWorldInit(&game->world);
  game->local_player = ShroomWorldSpawnPlayer(&game->world, 1, false);
  for (bot_index = 0; bot_index < SHROOM_BOT_COUNT; ++bot_index) {
    ShroomWorldSpawnPlayer(&game->world, (ShroomPlayerId)(bot_index + 2), true);
  }

  game->camera.offset = (Vector2){screen_width / 2.0f, screen_height / 2.0f};
  game->camera.target = (Vector2){game->local_player->position.x, game->local_player->position.y};
  game->camera.rotation = 0.0f;
  game->camera.zoom = 1.0f;
}

void GameUpdate(Game* game, float delta_time) {
  const ShroomVec2 input_direction = GetMovementInput(game);

  ClientNetUpdate(&game->net, input_direction, delta_time);

  if (game->net.welcome_received && (game->net.snapshot_player_count > 0)) {
    ApplyNetworkSnapshot(game);
  } else {
    ShroomPlayerSetInput(game->local_player, input_direction);
    ShroomWorldStep(&game->world, delta_time);
  }

  game->camera.target = (Vector2){game->local_player->position.x, game->local_player->position.y};
}

void GameDraw(const Game* game) {
  LeaderboardEntry leaderboard[SHROOM_MAX_PLAYERS];
  size_t leaderboard_count = 0;
  size_t index;
  size_t shown_count;
  const ShroomZone zone = ShroomGetZoneAtPosition(&game->world, game->local_player->position);

  BuildLeaderboard(game, leaderboard, &leaderboard_count);

  BeginDrawing();
  ClearBackground((Color){18, 18, 26, 255});

  BeginMode2D(game->camera);

  DrawArenaZones(&game->world);
  DrawRectangleLines(0, 0, (int)game->world.width, (int)game->world.height, Fade(DARKGREEN, 0.7f));
  DrawGrid(80, 64.0f);
  DrawSpores(&game->world);
  DrawPlayers(game);

  EndMode2D();

  DrawRectangle(24, 24, 510, 176, Fade(BLACK, 0.42f));
  DrawRectangle(1040, 24, 216, 176, Fade(BLACK, 0.42f));
  DrawText("shroomio", 40, 32, 40, RAYWHITE);
  DrawText("Move toward the mouse. WASD is temporary prototype input.", 40, 82, 20, GRAY);
  DrawText(TextFormat("Mass: %.0f  Radius: %.1f  Speed: %.0f", game->local_player->mass,
                      game->local_player->radius, ShroomMassToSpeed(game->local_player->mass)),
           40, 110, 20, GRAY);
  DrawText(TextFormat("Zone: %s  Players: %d  Spores: %d  Tick: %llu", GetZoneLabel(zone),
                      (int)game->world.player_count, (int)game->world.spore_count,
                      game->world.tick),
           40, 136, 20, GRAY);
  DrawText(TextFormat("Server: %s  Snapshot Tick: %llu", ClientNetStatusLabel(&game->net),
                      game->net.last_snapshot_tick),
           40, 162, 20, game->net.status == CLIENT_NET_CONNECTED ? GREEN : ORANGE);

  DrawText("Leaderboard", 1070, 36, 24, RAYWHITE);
  shown_count = leaderboard_count < 6 ? leaderboard_count : 6;
  for (index = 0; index < shown_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[leaderboard[index].index];
    const char* label =
        (player == game->local_player) ? "You" : (player->is_bot ? "Bot" : "Player");

    DrawText(
        TextFormat("%d. %s %u   %.0f", (int)(index + 1), label, player->player_id, player->mass),
        1062, 72 + ((int)index * 22), 20, player == game->local_player ? RAYWHITE : GRAY);
  }

  EndDrawing();
}

void GameShutdown(Game* game) { ClientNetShutdown(&game->net); }
