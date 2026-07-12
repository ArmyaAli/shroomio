#include "game.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "audio.h"
#include "cursor.h"
#include "imgui_wrapper.h"
#include "match_feedback.h"
#include "raymath.h"
#include "render_lod.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include "shared/profiler.h"
#include "shared/sim.h"

static const Color kBotColors[] = {
    {255, 173, 96, 255},  {255, 99, 132, 255},  {122, 162, 247, 255},
    {186, 104, 200, 255}, {255, 214, 102, 255},
};

static const float kRemoteInterpolationRate = 10.0f;
static const float kStatusBannerDuration = 2.0f;
static const float kProximityMapRadius = 74.0f;
static const float kProximityMapRange = 1400.0f;
static const float kRenderCullMargin = 96.0f;

static const char* GetPlayerDisplayName(const Game* game, const ShroomPlayerState* player);
static int GetPlayerRank(const Game* game, const ShroomPlayerState* target_player);
static Color GetPowerupColor(ShroomPowerupType type);
static Color GetZoneColor(ShroomZone zone);
static const char* GetZoneSummary(ShroomZone zone);
static const char* GetZoneLabel(ShroomZone zone);
static const ShroomPlayerState* GetInputReferencePlayer(const Game* game);

static bool IsOnlineMode(GameSessionMode mode) {
  return mode == SHROOM_SESSION_MODE_QUICK_PLAY || mode == SHROOM_SESSION_MODE_LOBBY_PLAY;
}

static Rectangle GetCameraWorldBounds(Camera2D camera) {
  const Vector2 top_left = GetScreenToWorld2D((Vector2){0.0f, 0.0f}, camera);
  const Vector2 bottom_right =
      GetScreenToWorld2D((Vector2){(float)GetScreenWidth(), (float)GetScreenHeight()}, camera);
  const float min_x = fminf(top_left.x, bottom_right.x) - kRenderCullMargin;
  const float min_y = fminf(top_left.y, bottom_right.y) - kRenderCullMargin;
  const float max_x = fmaxf(top_left.x, bottom_right.x) + kRenderCullMargin;
  const float max_y = fmaxf(top_left.y, bottom_right.y) + kRenderCullMargin;

  return (Rectangle){min_x, min_y, max_x - min_x, max_y - min_y};
}

static Rectangle ClampRectToWorld(Rectangle rect, const ShroomWorldState* world) {
  const float min_x = fmaxf(rect.x, 0.0f);
  const float min_y = fmaxf(rect.y, 0.0f);
  const float max_x = fminf(rect.x + rect.width, world->width);
  const float max_y = fminf(rect.y + rect.height, world->height);

  if ((max_x <= min_x) || (max_y <= min_y)) {
    return (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
  }

  return (Rectangle){min_x, min_y, max_x - min_x, max_y - min_y};
}

static bool CircleIntersectsRect(Vector2 center, float radius, Rectangle rect) {
  return (center.x + radius >= rect.x) && (center.x - radius <= rect.x + rect.width) &&
         (center.y + radius >= rect.y) && (center.y - radius <= rect.y + rect.height);
}

static bool IsPositionInBoundaryConsumeZone(const ShroomWorldState* world, ShroomVec2 position) {
  if (world == NULL) {
    return false;
  }
  return position.x < SHROOM_BOUNDARY_CONSUME_MARGIN ||
         position.x > (world->width - SHROOM_BOUNDARY_CONSUME_MARGIN) ||
         position.y < SHROOM_BOUNDARY_CONSUME_MARGIN ||
         position.y > (world->height - SHROOM_BOUNDARY_CONSUME_MARGIN);
}

static float GetBoundaryConsumeWarningIntensity(const ShroomWorldState* world,
                                                ShroomVec2 position) {
  float nearest_edge;

  if (world == NULL) {
    return 0.0f;
  }

  nearest_edge = fminf(fminf(position.x, world->width - position.x),
                       fminf(position.y, world->height - position.y));
  if (nearest_edge >= SHROOM_BOUNDARY_CONSUME_MARGIN) {
    return 0.0f;
  }

  return Clamp((SHROOM_BOUNDARY_CONSUME_MARGIN - nearest_edge) / SHROOM_BOUNDARY_CONSUME_MARGIN,
               0.0f, 1.0f);
}

static int GetParticleBudget(ClientParticleQuality quality) {
  switch (quality) {
  case CLIENT_PARTICLES_OFF:
    return 0;
  case CLIENT_PARTICLES_LOW:
    return 96;
  case CLIENT_PARTICLES_HIGH:
    return (int)SHROOM_CLIENT_PARTICLE_CAPACITY;
  case CLIENT_PARTICLES_MEDIUM:
  default:
    return 192;
  }
}

static int ScaleParticleCount(const Game* game, int high_count) {
  switch (game->settings.particle_quality) {
  case CLIENT_PARTICLES_OFF:
    return 0;
  case CLIENT_PARTICLES_LOW:
    return (high_count + 3) / 4;
  case CLIENT_PARTICLES_MEDIUM:
    return (high_count + 1) / 2;
  case CLIENT_PARTICLES_HIGH:
  default:
    return high_count;
  }
}

static float RandomRange(float min_value, float max_value) {
  const float t = (float)GetRandomValue(0, 10000) / 10000.0f;
  return min_value + ((max_value - min_value) * t);
}

static float SmoothStep01(float value) {
  const float t = Clamp(value, 0.0f, 1.0f);
  return t * t * (3.0f - (2.0f * t));
}

static float ClampPanelPosition(float value, float panel_size, float screen_size, float margin) {
  if (screen_size <= panel_size + margin * 2.0f) {
    return margin;
  }
  return Clamp(value, margin, screen_size - panel_size - margin);
}

void GamePlayUiClickSound(const Game* game) {
  if (game != NULL) {
    ShroomClientAudioPlaySfx(&game->settings, SHROOM_CLIENT_SFX_UI_CLICK, 0.46f);
  }
}

void GamePlayUiErrorSound(const Game* game) {
  if (game != NULL) {
    ShroomClientAudioPlaySfx(&game->settings, SHROOM_CLIENT_SFX_UI_ERROR, 0.58f);
  }
}

static void SpawnGameplayParticle(Game* game, Vector2 position, Vector2 velocity, Color color,
                                  float radius, float lifetime) {
  const int budget = GetParticleBudget(game->settings.particle_quality);
  GameplayParticle* particle;

  if (budget <= 0) {
    return;
  }

  particle = &game->particles[game->particle_cursor % (uint32_t)budget];
  *particle = (GameplayParticle){
      .position = position,
      .velocity = velocity,
      .color = color,
      .lifetime = lifetime,
      .radius = radius,
      .active = true,
  };
  game->particle_cursor = (game->particle_cursor + 1u) % (uint32_t)budget;
}

static void SpawnParticleBurst(Game* game, ShroomVec2 origin, Color color, int high_count,
                               float speed, float radius, float lifetime) {
  const int count = ScaleParticleCount(game, high_count);

  for (int index = 0; index < count; ++index) {
    const float angle = RandomRange(0.0f, 2.0f * PI);
    const float particle_speed = RandomRange(speed * 0.25f, speed);
    const Vector2 velocity = {cosf(angle) * particle_speed, sinf(angle) * particle_speed};
    SpawnGameplayParticle(game, (Vector2){origin.x, origin.y}, velocity, color,
                          RandomRange(radius * 0.55f, radius * 1.2f),
                          RandomRange(lifetime * 0.65f, lifetime * 1.15f));
  }
}

static void AddCombatNotification(Game* game, const char* title, const char* detail, Color color,
                                  float duration) {
  if (game->notification_count >= SHROOM_CLIENT_NOTIFICATION_CAPACITY) {
    game->notification_head = (game->notification_head + 1u) % SHROOM_CLIENT_NOTIFICATION_CAPACITY;
    game->notification_count -= 1u;
  }
  {
    const uint32_t slot =
        (game->notification_head + game->notification_count) % SHROOM_CLIENT_NOTIFICATION_CAPACITY;
    CombatNotification* notification = &game->notifications[slot];

    snprintf(notification->title, sizeof(notification->title), "%s", title);
    snprintf(notification->detail, sizeof(notification->detail), "%s", detail);
    notification->color = color;
    notification->age = 0.0f;
    notification->duration = fmaxf(duration, 4.0f);
    notification->active = true;
    game->notification_count += 1u;
  }
}

static uint64_t MakeKillFeedEventKey(ShroomEntityId killer_entity_id,
                                     ShroomEntityId victim_entity_id) {
  return ((uint64_t)killer_entity_id << 32u) | (uint64_t)victim_entity_id;
}

static void AddKillFeedEntry(Game* game, const char* text, Color color, uint64_t event_key) {
  if (event_key != 0ull) {
    for (uint32_t offset = 0u; offset < game->kill_feed_count; ++offset) {
      const uint32_t slot = (game->kill_feed_head + offset) % 8u;
      if (game->kill_feed[slot].active && (game->kill_feed[slot].event_key == event_key)) {
        return;
      }
    }
  }

  if (game->kill_feed_count >= 8u) {
    game->kill_feed_head = (game->kill_feed_head + 1u) % 8u;
    game->kill_feed_count -= 1u;
  }
  {
    const uint32_t slot = (game->kill_feed_head + game->kill_feed_count) % 8u;
    KillFeedEntry* entry = &game->kill_feed[slot];

    snprintf(entry->text, sizeof(entry->text), "%s", text);
    entry->event_key = event_key;
    entry->color = color;
    entry->age = 0.0f;
    entry->duration = 4.8f;
    entry->active = true;
    game->kill_feed_count += 1u;
  }
}

static void StartDeathCutscene(Game* game, const char* killer_name, float final_mass,
                               int final_rank) {
  if (!game->settings.death_cutscene_enabled) {
    return;
  }

  snprintf(game->death_cutscene_killer_name, sizeof(game->death_cutscene_killer_name), "%s",
           (killer_name != NULL && killer_name[0] != '\0') ? killer_name : "a larger colony");
  game->death_cutscene_timer = 0.0f;
  game->death_cutscene_duration = 2.8f;
  game->death_cutscene_final_mass = final_mass;
  game->death_cutscene_final_rank = final_rank;
  game->death_cutscene_survival_time = fmaxf(0.0f, (float)GetTime() - game->session_start_time);
}

static GameplayEvent* PushGameplayEvent(Game* game, GameplayEventType type) {
  GameplayEvent* event;
  uint32_t slot;

  if (game->gameplay_event_count >= SHROOM_CLIENT_GAMEPLAY_EVENT_CAPACITY) {
    game->gameplay_event_head =
        (game->gameplay_event_head + 1u) % SHROOM_CLIENT_GAMEPLAY_EVENT_CAPACITY;
    game->gameplay_event_count -= 1u;
  }

  slot = (game->gameplay_event_head + game->gameplay_event_count) %
         SHROOM_CLIENT_GAMEPLAY_EVENT_CAPACITY;
  event = &game->gameplay_events[slot];
  memset(event, 0, sizeof(*event));
  event->type = type;
  game->gameplay_event_count += 1u;
  return event;
}

static void QueueGameplayParticleBurst(Game* game, ShroomVec2 position, Color color, int count,
                                       float speed, float radius, float lifetime) {
  GameplayEvent* event = PushGameplayEvent(game, GAMEPLAY_EVENT_PARTICLE_BURST);
  event->position = position;
  event->color = color;
  event->count = count;
  event->speed = speed;
  event->radius = radius;
  event->lifetime = lifetime;
}

static void QueueGameplayNotification(Game* game, const char* title, const char* detail,
                                      Color color, float duration) {
  GameplayEvent* event = PushGameplayEvent(game, GAMEPLAY_EVENT_NOTIFICATION);
  snprintf(event->title, sizeof(event->title), "%s", title);
  snprintf(event->detail, sizeof(event->detail), "%s", detail);
  event->color = color;
  event->duration = duration;
}

static void QueueGameplayScreenFlash(Game* game, Color color, float duration) {
  GameplayEvent* event = PushGameplayEvent(game, GAMEPLAY_EVENT_SCREEN_FLASH);
  event->color = color;
  event->duration = duration;
}

static void QueueGameplaySfx(Game* game, ShroomClientSfx sfx, float importance) {
  GameplayEvent* event = PushGameplayEvent(game, GAMEPLAY_EVENT_SFX);
  event->sfx = (int)sfx;
  event->importance = importance;
}

static void QueueGameplayDeathCutscene(Game* game, const char* killer_name, float final_mass,
                                       int final_rank) {
  GameplayEvent* event = PushGameplayEvent(game, GAMEPLAY_EVENT_DEATH_CUTSCENE);
  snprintf(event->name, sizeof(event->name), "%s", killer_name);
  event->final_mass = final_mass;
  event->final_rank = final_rank;
}

static void QueueGameplayZoneCallout(Game* game, ShroomZone zone) {
  GameplayEvent* event = PushGameplayEvent(game, GAMEPLAY_EVENT_ZONE_CALLOUT);
  event->position = game->local_player != NULL ? game->local_player->position : (ShroomVec2){0};
  event->color = GetZoneColor(zone);
  snprintf(event->title, sizeof(event->title), "Entered %s zone", GetZoneLabel(zone));
  snprintf(event->detail, sizeof(event->detail), "%s", GetZoneSummary(zone));
}

static void QueueGameplayRespawnBanner(Game* game) {
  (void)PushGameplayEvent(game, GAMEPLAY_EVENT_RESPAWN_BANNER);
}

static void DispatchGameplayEvent(Game* game, const GameplayEvent* event) {
  switch (event->type) {
  case GAMEPLAY_EVENT_PARTICLE_BURST:
    SpawnParticleBurst(game, event->position, event->color, event->count, event->speed,
                       event->radius, event->lifetime);
    break;
  case GAMEPLAY_EVENT_NOTIFICATION:
    AddCombatNotification(game, event->title, event->detail, event->color, event->duration);
    break;
  case GAMEPLAY_EVENT_SCREEN_FLASH:
    game->screen_flash_color = event->color;
    game->screen_flash_timer = event->duration;
    break;
  case GAMEPLAY_EVENT_SFX:
    ShroomClientAudioPlaySfx(&game->settings, (ShroomClientSfx)event->sfx, event->importance);
    break;
  case GAMEPLAY_EVENT_DEATH_CUTSCENE:
    StartDeathCutscene(game, event->name, event->final_mass, event->final_rank);
    break;
  case GAMEPLAY_EVENT_ZONE_CALLOUT:
    SpawnParticleBurst(game, event->position, event->color, 12, 86.0f, 5.0f, 0.64f);
    AddCombatNotification(game, event->title, event->detail, event->color, 2.4f);
    ShroomClientAudioPlaySfx(&game->settings, SHROOM_CLIENT_SFX_ZONE, 0.62f);
    game->zone_callout_timer = kStatusBannerDuration;
    break;
  case GAMEPLAY_EVENT_RESPAWN_BANNER:
    game->respawn_banner_timer = kStatusBannerDuration;
    break;
  }
}

static void DispatchQueuedGameplayEvents(Game* game) {
  while (game->gameplay_event_count > 0u) {
    const GameplayEvent event = game->gameplay_events[game->gameplay_event_head];
    game->gameplay_event_head =
        (game->gameplay_event_head + 1u) % SHROOM_CLIENT_GAMEPLAY_EVENT_CAPACITY;
    game->gameplay_event_count -= 1u;
    DispatchGameplayEvent(game, &event);
  }
}

static const ShroomPlayerState* FindLargestMassGainer(const Game* game, float* mass_gain) {
  const ShroomPlayerState* best_player = NULL;
  float best_gain = 0.0f;

  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    float previous_mass = 0.0f;
    bool had_same_entity = false;

    for (size_t previous_index = 0; previous_index < SHROOM_MAX_PLAYERS; ++previous_index) {
      if (game->previous_player_alive[previous_index] &&
          (game->previous_player_entity_ids[previous_index] == player->entity_id)) {
        previous_mass = game->previous_player_masses[previous_index];
        had_same_entity = true;
        break;
      }
    }

    if (!player->alive || !had_same_entity) {
      continue;
    }
    const float gain = player->mass - previous_mass;
    if (gain > best_gain) {
      best_gain = gain;
      best_player = player;
    }
  }

  if (mass_gain != NULL) {
    *mass_gain = best_gain;
  }
  return best_player;
}

static bool FindPreviousPlayerMassByEntityId(const Game* game, ShroomEntityId entity_id,
                                             float* previous_mass) {
  if (entity_id == 0u) {
    return false;
  }
  for (size_t index = 0; index < SHROOM_MAX_PLAYERS; ++index) {
    if (game->previous_player_alive[index] &&
        (game->previous_player_entity_ids[index] == entity_id)) {
      if (previous_mass != NULL) {
        *previous_mass = game->previous_player_masses[index];
      }
      return true;
    }
  }
  return false;
}

static const ShroomPlayerState* FindCurrentPlayerByEntityId(const Game* game,
                                                            ShroomEntityId entity_id) {
  if (entity_id == 0u) {
    return NULL;
  }
  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    if (player->entity_id == entity_id) {
      return player;
    }
  }
  return NULL;
}

static const ShroomPlayerState* InferConsumeKiller(const Game* game, size_t previous_victim_index,
                                                   float* killer_gain) {
  const float victim_mass = game->previous_player_masses[previous_victim_index];
  const ShroomVec2 victim_pos = game->previous_player_positions[previous_victim_index];
  const float expected_gain = victim_mass * SHROOM_CONSUME_MASS_GAIN_FACTOR;
  const float minimum_gain = fmaxf(3.0f, expected_gain * 0.20f);
  const ShroomPlayerId victim_player_id = game->previous_player_ids[previous_victim_index];
  const ShroomPlayerState* best_player = NULL;
  float best_gain = 0.0f;
  float best_score = 0.0f;

  if (victim_mass <= 0.0f) {
    return NULL;
  }

  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    float previous_mass = 0.0f;
    float gain;
    float distance_sqr;
    float score;

    if (!player->alive || (player->player_id == victim_player_id) ||
        !FindPreviousPlayerMassByEntityId(game, player->entity_id, &previous_mass)) {
      continue;
    }

    gain = player->mass - previous_mass;
    if (gain < minimum_gain) {
      continue;
    }

    distance_sqr = ShroomDistanceSqr(player->position, victim_pos);
    score = gain - (sqrtf(distance_sqr) * 0.01f);

    if ((best_player == NULL) || (score > best_score)) {
      best_player = player;
      best_gain = gain;
      best_score = score;
    }
  }

  if (killer_gain != NULL) {
    *killer_gain = best_gain;
  }
  return best_player;
}

static size_t FindPreviousLocalPrimaryIndex(const Game* game, ShroomPlayerId local_player_id) {
  if (local_player_id == 0u) {
    return SHROOM_MAX_PLAYERS;
  }

  for (size_t index = 0; index < SHROOM_MAX_PLAYERS; ++index) {
    if (game->previous_player_alive[index] &&
        (game->previous_player_ids[index] == local_player_id) &&
        (game->previous_player_piece_indices[index] == 0u)) {
      return index;
    }
  }

  return SHROOM_MAX_PLAYERS;
}

static const ShroomPlayerState* FindCurrentLocalPrimary(const Game* game,
                                                        ShroomPlayerId local_player_id) {
  if (local_player_id == 0u) {
    return NULL;
  }

  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    if (player->alive && (player->player_id == local_player_id) && (player->piece_index == 0u)) {
      return player;
    }
  }

  return NULL;
}

static void UpdateCombatNotifications(Game* game, float delta_time) {
  if (game->notification_count > 0u) {
    CombatNotification* notification = &game->notifications[game->notification_head];
    notification->age += delta_time;
    if (notification->age >= notification->duration) {
      notification->active = false;
      game->notification_head =
          (game->notification_head + 1u) % SHROOM_CLIENT_NOTIFICATION_CAPACITY;
      game->notification_count -= 1u;
    }
  }

  for (uint32_t offset = 0u; offset < game->kill_feed_count; ++offset) {
    const uint32_t slot = (game->kill_feed_head + offset) % 8u;
    KillFeedEntry* entry = &game->kill_feed[slot];
    if (entry->active) {
      entry->age += delta_time;
    }
  }
  while (game->kill_feed_count > 0u) {
    KillFeedEntry* entry = &game->kill_feed[game->kill_feed_head];
    if (entry->active && (entry->age < entry->duration)) {
      break;
    }
    entry->active = false;
    game->kill_feed_head = (game->kill_feed_head + 1u) % 8u;
    game->kill_feed_count -= 1u;
  }

  if (game->screen_flash_timer > 0.0f) {
    game->screen_flash_timer = fmaxf(0.0f, game->screen_flash_timer - delta_time);
  }
  if (game->combat_feedback_cooldown > 0.0f) {
    game->combat_feedback_cooldown = fmaxf(0.0f, game->combat_feedback_cooldown - delta_time);
  }
}

static void DrawCombatNotifications(const Game* game) {
  const CombatNotification* notification;
  float alpha;
  float y;
  Rectangle panel;

  if (game->screen_flash_timer > 0.0f) {
    const float flash_alpha = fminf(game->screen_flash_timer / 0.34f, 1.0f) * 0.24f;
    DrawRectangle(0, 0, game->screen_width, game->screen_height,
                  Fade(game->screen_flash_color, flash_alpha));
  }

  if (game->notification_count == 0u) {
    return;
  }

  notification = &game->notifications[game->notification_head];

  alpha = 1.0f;
  if (notification->age < 0.24f) {
    alpha = SmoothStep01(notification->age / 0.24f);
  } else if (notification->duration - notification->age < 0.90f) {
    alpha = SmoothStep01(fmaxf(0.0f, (notification->duration - notification->age) / 0.90f));
  }

  y = 24.0f;
  if (notification->age < 0.24f) {
    y -= (1.0f - SmoothStep01(notification->age / 0.24f)) * 18.0f;
  } else if (notification->duration - notification->age < 0.90f) {
    y += (1.0f - alpha) * 12.0f;
  }

  panel = (Rectangle){ClampPanelPosition((game->screen_width - 440.0f) * 0.5f, 440.0f,
                                         (float)game->screen_width, 12.0f),
                      y, 440.0f, 58.0f};
  DrawRectangleRounded(panel, 0.22f, 8, Fade((Color){18, 21, 18, 255}, 0.78f * alpha));
  DrawRectangleRoundedLines(panel, 0.22f, 8, Fade(notification->color, 0.82f * alpha));
  DrawCircleV((Vector2){panel.x + 24.0f, panel.y + 29.0f}, 9.0f,
              Fade(notification->color, 0.86f * alpha));
  DrawText(notification->title, (int)(panel.x + 46.0f), (int)(panel.y + 10.0f), 18,
           Fade(RAYWHITE, alpha));
  DrawText(notification->detail, (int)(panel.x + 46.0f), (int)(panel.y + 34.0f), 13,
           Fade((Color){226, 232, 210, 255}, 0.86f * alpha));
}

static void DrawKillFeed(const Game* game) {
  const float row_width = fminf(320.0f, (float)game->screen_width - 36.0f);
  const float row_height = 28.0f;
  const float row_gap = 6.0f;
  const uint32_t shown_entries = game->kill_feed_count < 4u ? game->kill_feed_count : 4u;
  const float x = (float)game->screen_width - row_width - 18.0f;
  float bottom_margin = 22.0f;

  if (shown_entries == 0u) {
    return;
  }
  if (game->diagnostics_overlay_open) {
    bottom_margin += 210.0f;
  } else if (game->chat_open && IsOnlineMode(game->active_mode)) {
    bottom_margin += 18.0f;
  }

  for (uint32_t offset = 0u; offset < shown_entries; ++offset) {
    const uint32_t slot = (game->kill_feed_head + game->kill_feed_count - 1u - offset) % 8u;
    const KillFeedEntry* entry = &game->kill_feed[slot];
    float alpha = 1.0f;
    const float y = (float)game->screen_height - bottom_margin -
                    ((float)(offset + 1u) * row_height) - ((float)offset * row_gap);
    Rectangle panel;

    if (!entry->active) {
      continue;
    }
    if (entry->age < 0.18f) {
      alpha = SmoothStep01(entry->age / 0.18f);
    } else if (entry->duration - entry->age < 0.90f) {
      alpha = SmoothStep01(fmaxf(0.0f, (entry->duration - entry->age) / 0.90f));
    }

    panel = (Rectangle){x, y, row_width, row_height};
    DrawRectangleRounded(panel, 0.26f, 8, Fade((Color){16, 21, 16, 255}, 0.54f * alpha));
    DrawRectangleRoundedLines(panel, 0.26f, 8, Fade(entry->color, 0.58f * alpha));
    DrawCircleV((Vector2){panel.x + 14.0f, panel.y + 14.0f}, 4.5f, Fade(entry->color, alpha));
    DrawText(entry->text, (int)(panel.x + 26.0f), (int)(panel.y + 7.0f), 14,
             Fade(RAYWHITE, 0.90f * alpha));
  }
}

static bool IsDeathCutsceneOpen(const Game* game) {
  return (game != NULL) && (game->death_cutscene_duration > 0.0f) &&
         (game->death_cutscene_timer < game->death_cutscene_duration + 20.0f);
}

static const ShroomPlayerState* FindSpectatorTarget(const Game* game) {
  if (game == NULL) {
    return NULL;
  }
  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    if (player->alive && (player->entity_id == game->spectated_entity_id)) {
      return player;
    }
  }
  return NULL;
}

static const ShroomLobbyEntry* FindCurrentLobbyEntry(const Game* game, int* current_index) {
  if (current_index != NULL) {
    *current_index = -1;
  }
  if ((game == NULL) || (game->net.lobby_id == 0u)) {
    return NULL;
  }
  for (uint8_t index = 0; index < game->net.lobby_count; ++index) {
    const ShroomLobbyEntry* lobby = &game->net.lobby_list[index];
    if (lobby->lobby_id == game->net.lobby_id) {
      if (current_index != NULL) {
        *current_index = (int)index;
      }
      return lobby;
    }
  }
  return NULL;
}

static void GameSwitchSpectatorLobby(Game* game, int direction) {
  int current_index = -1;
  int next_index;

  if ((game == NULL) || !game->spectator_mode || !IsOnlineMode(game->active_mode) ||
      (game->net.lobby_count <= 1u)) {
    return;
  }

  FindCurrentLobbyEntry(game, &current_index);
  if (current_index < 0) {
    current_index = direction < 0 ? 0 : (int)game->net.lobby_count - 1;
  }

  next_index = (current_index + (int)game->net.lobby_count + (direction < 0 ? -1 : 1)) %
               (int)game->net.lobby_count;
  if (game->net.lobby_list[next_index].lobby_id == game->net.lobby_id) {
    return;
  }

  ClientNetSendLobbyLeave(&game->net);
  ClientNetSendLobbyJoin(&game->net, game->net.lobby_list[next_index].lobby_id, true);
  game->spectated_entity_id = 0;
  game->spectator_follow_mode = true;
}

void GameCycleSpectatorTarget(Game* game, int direction) {
  uint32_t targets[SHROOM_MAX_PLAYERS];
  int target_count = 0;
  int current_index = -1;

  if (game == NULL) {
    return;
  }
  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    if (!player->alive || (player == game->local_player)) {
      continue;
    }
    if (target_count < SHROOM_MAX_PLAYERS) {
      if (player->entity_id == game->spectated_entity_id) {
        current_index = target_count;
      }
      targets[target_count++] = player->entity_id;
    }
  }
  if (target_count <= 0) {
    game->spectated_entity_id = 0;
    game->spectator_follow_mode = false;
    return;
  }
  if (current_index < 0) {
    current_index = direction < 0 ? 0 : target_count - 1;
  }
  game->spectated_entity_id =
      targets[(current_index + target_count + (direction < 0 ? -1 : 1)) % target_count];
  game->spectator_follow_mode = true;
}

void GameEnterSpectatorMode(Game* game) {
  if (game == NULL) {
    return;
  }
  game->spectator_mode = true;
  game->spectator_follow_mode = true;
  game->death_cutscene_timer = 0.0f;
  game->death_cutscene_duration = 0.0f;
  game->death_camera_hold_timer = 0.0f;
  game->split_requested = false;
  game->split_hold_timer = 0.0f;
  if (FindSpectatorTarget(game) == NULL) {
    GameCycleSpectatorTarget(game, 1);
  }
}

void GameExitSpectatorMode(Game* game) {
  if (game == NULL) {
    return;
  }
  game->spectator_mode = false;
  game->spectator_follow_mode = false;
  game->spectated_entity_id = 0;
}

static void UpdateDeathCutscene(Game* game, float delta_time) {
  if (!IsDeathCutsceneOpen(game)) {
    return;
  }
  if (game->death_cutscene_timer < game->death_cutscene_duration) {
    game->death_cutscene_timer += delta_time;
  }
}

static void DrawDeathCutscene(Game* game) {
  const float duration = game->death_cutscene_duration;
  float progress;
  float squash;
  Vector2 center;
  Color vignette = (Color){42, 10, 24, 255};
  char killer_text[96];
  char stats_text[128];

  if (!IsDeathCutsceneOpen(game) || (duration <= 0.0f)) {
    return;
  }

  progress = fminf(game->death_cutscene_timer / duration, 1.0f);
  squash = sinf(progress * PI);
  center = (Vector2){game->screen_width * 0.5f, game->screen_height * 0.42f};
  snprintf(killer_text, sizeof(killer_text), "by %s", game->death_cutscene_killer_name);
  snprintf(stats_text, sizeof(stats_text), "Mass %.0f   Rank %d   Survived %.0fs",
           game->death_cutscene_final_mass, game->death_cutscene_final_rank,
           game->death_cutscene_survival_time);

  DrawRectangle(0, 0, game->screen_width, game->screen_height,
                Fade(vignette, 0.58f + progress * 0.18f));
  DrawCircleGradient((int)center.x, (int)center.y, 280.0f + squash * 70.0f,
                     Fade((Color){110, 28, 52, 255}, 0.72f), Fade(BLACK, 0.0f));

  for (int index = 0; index < 18; ++index) {
    const float angle = ((float)index / 18.0f) * 2.0f * PI + progress * 1.2f;
    const float radius = 56.0f + progress * 210.0f + sinf(progress * 12.0f + index) * 10.0f;
    const Vector2 spore = {center.x + cosf(angle) * radius, center.y + sinf(angle) * radius};
    DrawCircleV(spore, 5.0f + squash * 3.0f,
                Fade((Color){255, 218, 120, 255}, 1.0f - progress * 0.45f));
  }

  DrawEllipse((int)center.x, (int)(center.y + 42.0f), 90.0f + squash * 18.0f,
              28.0f - squash * 12.0f, Fade((Color){72, 36, 26, 255}, 0.96f));
  DrawEllipse((int)center.x, (int)center.y, 86.0f + squash * 36.0f, 70.0f - squash * 42.0f,
              Fade((Color){212, 58, 72, 255}, 0.96f));
  DrawCircleV((Vector2){center.x - 28.0f, center.y - 16.0f}, 13.0f, Fade(RAYWHITE, 0.8f));
  DrawCircleV((Vector2){center.x + 24.0f, center.y - 20.0f}, 10.0f, Fade(RAYWHITE, 0.7f));
  DrawLineEx((Vector2){center.x - 34.0f, center.y + 28.0f},
             (Vector2){center.x + 36.0f, center.y + 22.0f}, 4.0f, Fade(BLACK, 0.58f));

  DrawText("YOU WERE CONSUMED", (game->screen_width - MeasureText("YOU WERE CONSUMED", 34)) / 2,
           (int)(center.y + 118.0f), 34, Fade((Color){255, 225, 196, 255}, 0.98f));
  DrawText(killer_text, (game->screen_width - MeasureText(killer_text, 20)) / 2,
           (int)(center.y + 158.0f), 20, Fade((Color){235, 190, 150, 255}, 0.92f));
  DrawText(stats_text, (game->screen_width - MeasureText(stats_text, 18)) / 2,
           (int)(center.y + 188.0f), 18, Fade(RAYWHITE, 0.86f));

  if (progress >= 1.0f) {
    ShroomImGui_SetNextWindowPos((game->screen_width - 360.0f) * 0.5f, center.y + 226.0f,
                                 SHROOM_IMGUI_COND_ALWAYS);
    ShroomImGui_SetNextWindowSize(360.0f, 126.0f, SHROOM_IMGUI_COND_ALWAYS);
    ShroomImGui_SetNextWindowBgAlpha(0.0f);
    if (ShroomImGui_Begin("Death Cutscene Actions", NULL,
                          SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                              SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                              SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
      if (ShroomImGui_Button("Play Again", 160.0f, 36.0f)) {
        game->play_again_requested = true;
      }
      ShroomImGui_SameLine();
      if (ShroomImGui_Button("Spectate", 170.0f, 36.0f)) {
        GameEnterSpectatorMode(game);
      }
      if (ShroomImGui_Button("Return To Menu", 170.0f, 32.0f)) {
        game->return_to_menu_requested = true;
      }
      ShroomImGui_Text("Esc skips the animation. Spectate follows another colony.");
    }
    ShroomImGui_End();
  }
}

static void CaptureParticleBaselines(Game* game) {
  size_t index;

  for (index = 0; index < SHROOM_MAX_SPORES; ++index) {
    const ShroomSporeState* spore = &game->world.spores[index];
    game->previous_spore_entity_ids[index] = spore->entity_id;
    game->previous_spore_positions[index] = spore->position;
  }
  for (index = 0; index < SHROOM_MAX_PLAYERS; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    game->previous_player_entity_ids[index] = player->entity_id;
    game->previous_player_ids[index] = player->player_id;
    snprintf(game->previous_player_names[index], sizeof(game->previous_player_names[index]), "%s",
             player->name[0] != '\0' ? player->name : "Unknown");
    game->previous_player_positions[index] = player->position;
    game->previous_player_masses[index] = player->mass;
    game->previous_player_alive[index] = player->alive;
    game->previous_player_piece_indices[index] = player->piece_index;
  }
  for (index = 0; index < SHROOM_MAX_POWERUPS; ++index) {
    const ShroomPowerupState* powerup = &game->world.powerups[index];
    game->previous_powerup_entity_ids[index] = powerup->entity_id;
    game->previous_powerup_positions[index] = powerup->position;
    game->previous_powerup_active[index] = powerup->active;
  }
  game->particle_baseline_ready = true;
}

static void RebaselineMatchPresentation(Game* game) {
  memset(game->particles, 0, sizeof(game->particles));
  game->particle_cursor = 0u;

  memset(game->notifications, 0, sizeof(game->notifications));
  game->notification_head = 0u;
  game->notification_count = 0u;
  memset(game->kill_feed, 0, sizeof(game->kill_feed));
  game->kill_feed_head = 0u;
  game->kill_feed_count = 0u;
  memset(game->gameplay_events, 0, sizeof(game->gameplay_events));
  game->gameplay_event_head = 0u;
  game->gameplay_event_count = 0u;

  game->screen_flash_timer = 0.0f;
  game->screen_flash_color = (Color){0};
  game->death_cutscene_timer = 0.0f;
  game->death_cutscene_duration = 0.0f;
  game->death_cutscene_final_mass = 0.0f;
  game->death_cutscene_survival_time = 0.0f;
  game->death_cutscene_final_rank = 0;
  game->death_cutscene_killer_name[0] = '\0';
  game->death_camera_hold_timer = 0.0f;
  game->death_camera_hold_pos = (Vector2){0};
  game->zone_callout_timer = 0.0f;
  game->respawn_banner_timer = 0.0f;
  game->combat_feedback_cooldown = 0.0f;
  game->previous_local_rank = 0;

  memset(game->render_positions, 0, sizeof(game->render_positions));
  memset(game->render_position_initialized, 0, sizeof(game->render_position_initialized));
  game->focused_piece_entity_id = 0u;
  game->piece_focus_changed = false;
  game->local_has_split = false;
  game->local_piece_count = game->local_player != NULL ? 1 : 0;
  game->split_requested = false;
  game->eject_requested = false;
  game->split_hold_timer = 0.0f;

  game->pending_input_count = 0u;
  game->tracked_input_sequence = game->net.last_processed_input_sequence;
  if (game->local_player != NULL) {
    game->previous_local_position = game->local_player->position;
    game->previous_local_mass = game->local_player->mass;
    game->current_zone = ShroomGetZoneAtPosition(&game->world, game->local_player->position);
  } else {
    game->previous_local_position = (ShroomVec2){0};
    game->previous_local_mass = 0.0f;
  }

  CaptureParticleBaselines(game);
}

static void EmitGameplayEventParticles(Game* game) {
  size_t index;
  const ShroomPlayerId local_player_id =
      IsOnlineMode(game->active_mode)
          ? game->net.player_id
          : (game->local_player != NULL ? game->local_player->player_id : 0u);
  float largest_gain = 0.0f;
  const ShroomPlayerState* largest_gainer = FindLargestMassGainer(game, &largest_gain);
  const size_t previous_local_primary_index = FindPreviousLocalPrimaryIndex(game, local_player_id);
  const ShroomPlayerState* current_local_primary = FindCurrentLocalPrimary(game, local_player_id);
  const bool had_previous_local_primary = previous_local_primary_index < SHROOM_MAX_PLAYERS;
  const bool local_primary_missing = had_previous_local_primary && (current_local_primary == NULL);
  const bool local_primary_entity_changed =
      had_previous_local_primary && (current_local_primary != NULL) &&
      (current_local_primary->entity_id !=
       game->previous_player_entity_ids[previous_local_primary_index]);
  const bool local_primary_respawned =
      had_previous_local_primary && (current_local_primary != NULL) &&
      (current_local_primary->mass <= SHROOM_DEFAULT_PLAYER_MASS * 1.05f) &&
      (ShroomDistanceSqr(current_local_primary->position,
                         game->previous_player_positions[previous_local_primary_index]) > 2500.0f);
  const bool local_primary_consumed =
      had_previous_local_primary && game->previous_player_alive[previous_local_primary_index] &&
      !IsDeathCutsceneOpen(game) &&
      (local_primary_missing || local_primary_entity_changed || local_primary_respawned);
  bool local_kill_feed_reported = false;
  bool local_kill_reported = false;
  bool local_death_reported = false;

  if (!game->particle_baseline_ready) {
    CaptureParticleBaselines(game);
    return;
  }

  for (index = 0; index < game->world.spore_count; ++index) {
    const ShroomSporeState* spore = &game->world.spores[index];
    if ((game->previous_spore_entity_ids[index] == spore->entity_id) && spore->active &&
        (ShroomDistanceSqr(game->previous_spore_positions[index], spore->position) > 4096.0f)) {
      QueueGameplayParticleBurst(game, game->previous_spore_positions[index],
                                 (Color){255, 228, 112, 255}, 5, 70.0f, 4.0f, 0.42f);
    }
  }

  for (index = 0; index < game->world.powerup_count; ++index) {
    const ShroomPowerupState* powerup = &game->world.powerups[index];
    if (game->previous_powerup_active[index] && !powerup->active &&
        (game->previous_powerup_entity_ids[index] == powerup->entity_id)) {
      QueueGameplayParticleBurst(game, game->previous_powerup_positions[index],
                                 GetPowerupColor(powerup->type), 18, 118.0f, 6.0f, 0.62f);
      if (game->local_player != NULL) {
        const float pickup_radius = game->local_player->radius + SHROOM_POWERUP_RADIUS + 48.0f;
        if (ShroomDistanceSqr(game->local_player->position,
                              game->previous_powerup_positions[index]) <
            (pickup_radius * pickup_radius)) {
          QueueGameplaySfx(game, SHROOM_CLIENT_SFX_POWERUP, 0.66f);
        }
      }
    }
  }

  if (local_primary_consumed) {
    const float previous_mass = game->previous_player_masses[previous_local_primary_index];
    float inferred_gain = 0.0f;
    const ShroomPlayerState* inferred_killer =
        InferConsumeKiller(game, previous_local_primary_index, &inferred_gain);
    const char* killer_name =
        inferred_killer != NULL ? GetPlayerDisplayName(game, inferred_killer) : "a larger colony";
    const uint64_t feed_key =
        MakeKillFeedEventKey(inferred_killer != NULL ? inferred_killer->entity_id : 0u,
                             game->previous_player_entity_ids[previous_local_primary_index]);
    char feed_text[128];

    const ShroomVec2 death_pos = game->previous_player_positions[previous_local_primary_index];
    snprintf(feed_text, sizeof(feed_text), "%s consumed You", killer_name);
    AddKillFeedEntry(game, feed_text, (Color){245, 84, 84, 255}, feed_key);
    game->death_camera_hold_timer = 0.6f;
    game->death_camera_hold_pos = (Vector2){death_pos.x, death_pos.y};
    QueueGameplayParticleBurst(game, game->previous_player_positions[previous_local_primary_index],
                               (Color){150, 45, 42, 255}, 16, 132.0f, 6.5f, 0.68f);
    QueueGameplayNotification(
        game, "You were consumed",
        TextFormat("Final mass %.0f  Rank %d  Survived %.0fs", previous_mass,
                   game->previous_local_rank > 0 ? game->previous_local_rank : 0,
                   fmaxf(0.0f, (float)GetTime() - game->session_start_time)),
        (Color){245, 84, 84, 255}, 3.6f);
    QueueGameplayDeathCutscene(game, killer_name, previous_mass,
                               game->previous_local_rank > 0 ? game->previous_local_rank : 0);
    QueueGameplayScreenFlash(game, (Color){180, 32, 42, 255}, 0.34f);
    QueueGameplaySfx(game, SHROOM_CLIENT_SFX_DEATH, 0.92f);
    local_death_reported = true;
  }

  for (index = 0; index < SHROOM_MAX_PLAYERS; ++index) {
    const ShroomPlayerState* current_player;
    const ShroomPlayerState* inferred_killer;
    float inferred_gain = 0.0f;
    bool victim_respawned = false;
    char feed_text[128];

    if (!game->previous_player_alive[index] || (game->previous_player_piece_indices[index] != 0u) ||
        (game->previous_player_ids[index] == 0u) ||
        ((local_player_id != 0u) && (game->previous_player_ids[index] == local_player_id))) {
      continue;
    }

    current_player = FindCurrentPlayerByEntityId(game, game->previous_player_entity_ids[index]);
    if ((current_player != NULL) && current_player->alive) {
      victim_respawned = (current_player->mass <= SHROOM_DEFAULT_PLAYER_MASS * 1.05f) &&
                         (ShroomDistanceSqr(current_player->position,
                                            game->previous_player_positions[index]) > 2500.0f);
    }
    if ((current_player != NULL) && current_player->alive && !victim_respawned) {
      continue;
    }

    inferred_killer = InferConsumeKiller(game, index, &inferred_gain);
    if (inferred_killer == NULL) {
      continue;
    }

    if ((local_player_id != 0u) && (inferred_killer->player_id == local_player_id)) {
      if (local_kill_feed_reported) {
        continue;
      }
      snprintf(feed_text, sizeof(feed_text), "You consumed %s", game->previous_player_names[index]);
      AddKillFeedEntry(game, feed_text, (Color){112, 224, 128, 255},
                       MakeKillFeedEventKey(inferred_killer->entity_id,
                                            game->previous_player_entity_ids[index]));
      local_kill_feed_reported = true;
    } else if (!local_death_reported) {
      snprintf(feed_text, sizeof(feed_text), "%s consumed %s",
               GetPlayerDisplayName(game, inferred_killer), game->previous_player_names[index]);
      AddKillFeedEntry(game, feed_text, (Color){210, 220, 210, 255},
                       MakeKillFeedEventKey(inferred_killer->entity_id,
                                            game->previous_player_entity_ids[index]));
    }
  }

  for (index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    const bool had_same_entity = game->previous_player_entity_ids[index] == player->entity_id;
    const bool had_same_player = game->previous_player_ids[index] == player->player_id;
    const float previous_mass = game->previous_player_masses[index];
    const bool was_local_player =
        (local_player_id != 0u) && (game->previous_player_ids[index] == local_player_id);
    const bool local_consumed_player = (largest_gainer != NULL) && (local_player_id != 0u) &&
                                       (largest_gainer->player_id == local_player_id) &&
                                       (largest_gain >= 10.0f) &&
                                       game->previous_player_alive[index] && !was_local_player &&
                                       (!player->alive || (had_same_player && !had_same_entity));

    const bool was_primary_piece = game->previous_player_piece_indices[index] == 0;

    if (game->previous_player_alive[index] && had_same_player && !had_same_entity &&
        was_primary_piece) {
      QueueGameplayParticleBurst(game, game->previous_player_positions[index],
                                 (Color){150, 45, 42, 255}, 28, 150.0f, 7.0f, 0.76f);
      QueueGameplayParticleBurst(game, player->position, (Color){122, 220, 118, 255}, 14, 82.0f,
                                 5.0f, 0.58f);
    } else if (game->previous_player_alive[index] && !player->alive) {
      QueueGameplayParticleBurst(game, game->previous_player_positions[index],
                                 (Color){190, 62, 64, 255}, 24, 132.0f, 7.0f, 0.72f);
    }

    if (local_consumed_player && !local_kill_reported && !local_death_reported) {
      char title[96];
      char detail[128];
      snprintf(title, sizeof(title), "You consumed %s", GetPlayerDisplayName(game, player));
      snprintf(detail, sizeof(detail), "Victim mass %.0f  +%.0f mass  Rank %d", previous_mass,
               largest_gain, GetPlayerRank(game, largest_gainer));
      if (!local_kill_feed_reported) {
        char feed_text[128];
        snprintf(feed_text, sizeof(feed_text), "You consumed %s",
                 GetPlayerDisplayName(game, player));
        AddKillFeedEntry(game, feed_text, (Color){112, 224, 128, 255},
                         MakeKillFeedEventKey(largest_gainer->entity_id,
                                              game->previous_player_entity_ids[index]));
        local_kill_feed_reported = true;
      }
      QueueGameplayNotification(game, title, detail, (Color){112, 224, 128, 255}, 3.0f);
      QueueGameplayScreenFlash(game, (Color){104, 220, 122, 255}, 0.24f);
      QueueGameplaySfx(game, SHROOM_CLIENT_SFX_CONSUME, 0.86f);
      local_kill_reported = true;
    }

    if (!player->alive) {
      continue;
    }
    if (had_same_entity && game->previous_player_alive[index] &&
        (player->mass >= previous_mass + 18.0f)) {
      QueueGameplayParticleBurst(game, player->position, (Color){118, 210, 255, 255}, 16, 96.0f,
                                 5.5f, 0.54f);
      if ((local_player_id != 0u) && (player->player_id == local_player_id) &&
          !local_kill_reported) {
        QueueGameplayNotification(game, TextFormat("+%.0f mass", player->mass - previous_mass),
                                  "Spores absorbed into the colony", (Color){255, 228, 112, 255},
                                  1.8f);
      }
    }
    if (had_same_entity && game->previous_player_alive[index] &&
        (player->mass > previous_mass + 0.5f) && (local_player_id != 0u) &&
        (player->player_id == local_player_id) && !local_kill_reported) {
      QueueGameplaySfx(game, SHROOM_CLIENT_SFX_SPORE, 0.42f);
    }
    if ((player->piece_index > 0u) &&
        (!game->previous_player_alive[index] || !had_same_entity || (previous_mass <= 0.0f))) {
      QueueGameplayParticleBurst(game, player->position, (Color){255, 215, 116, 255}, 10, 130.0f,
                                 4.6f, 0.58f);
    }
    if (had_same_entity && game->previous_player_alive[index] &&
        (player->mass < previous_mass * 0.72f) && (previous_mass >= SHROOM_SPLIT_MIN_MASS)) {
      QueueGameplayParticleBurst(game, player->position, (Color){255, 215, 116, 255}, 9, 112.0f,
                                 4.4f, 0.52f);
    }
  }

  DispatchQueuedGameplayEvents(game);
  CaptureParticleBaselines(game);
}

static void UpdateGameplayParticles(Game* game, float delta_time) {
  const int budget = GetParticleBudget(game->settings.particle_quality);

  if (budget <= 0) {
    for (size_t index = 0; index < SHROOM_CLIENT_PARTICLE_CAPACITY; ++index) {
      game->particles[index].active = false;
    }
    return;
  }

  for (int index = 0; index < budget; ++index) {
    GameplayParticle* particle = &game->particles[index];
    if (!particle->active) {
      continue;
    }
    particle->age += delta_time;
    if (particle->age >= particle->lifetime) {
      particle->active = false;
      continue;
    }
    particle->position.x += particle->velocity.x * delta_time;
    particle->position.y += particle->velocity.y * delta_time;
    particle->velocity = Vector2Scale(particle->velocity, 1.0f - fminf(delta_time * 2.4f, 0.82f));
  }
}

static void UpdateAmbientParticles(Game* game, float delta_time) {
  const int count = ScaleParticleCount(game, 3);
  const float interval = game->settings.particle_quality == CLIENT_PARTICLES_HIGH     ? 0.035f
                         : game->settings.particle_quality == CLIENT_PARTICLES_MEDIUM ? 0.065f
                         : game->settings.particle_quality == CLIENT_PARTICLES_LOW    ? 0.12f
                                                                                      : 1.0f;

  if ((count <= 0) || (game->local_player == NULL)) {
    return;
  }
  game->ambient_particle_timer += delta_time;
  if (game->ambient_particle_timer < interval) {
    return;
  }
  game->ambient_particle_timer = 0.0f;

  for (int index = 0; index < count; ++index) {
    const Vector2 offset = {RandomRange(-480.0f, 480.0f), RandomRange(-300.0f, 300.0f)};
    const Vector2 position = Vector2Add(game->camera.target, offset);
    const Vector2 velocity = {RandomRange(-8.0f, 8.0f), RandomRange(-18.0f, -4.0f)};
    SpawnGameplayParticle(game, position, velocity, (Color){156, 222, 152, 255}, 2.8f, 1.4f);
  }
}

static void DrawGameplayParticles(const Game* game, Rectangle view_bounds) {
  const int budget = GetParticleBudget(game->settings.particle_quality);

  for (int index = 0; index < budget; ++index) {
    const GameplayParticle* particle = &game->particles[index];
    const float progress = particle->lifetime > 0.0f ? particle->age / particle->lifetime : 1.0f;
    const float alpha = 1.0f - progress;
    const float radius = particle->radius * (0.7f + progress * 0.9f);

    if (!particle->active ||
        !CircleIntersectsRect(particle->position, radius + 2.0f, view_bounds)) {
      continue;
    }
    DrawCircleV(particle->position, radius + 2.0f, Fade(particle->color, alpha * 0.22f));
    DrawCircleV(particle->position, radius, Fade(particle->color, alpha * 0.86f));
  }
}

static bool IsLocalPlayerPiece(const Game* game, const ShroomPlayerState* player);
static bool IsFocusedPiece(const Game* game, const ShroomPlayerState* player);
static void DrawFungalHudPanel(Rectangle rect, Color accent);

static const char* GetPlayerDisplayName(const Game* game, const ShroomPlayerState* player) {
  static char fallback_name[32];

  if (player == NULL) {
    return "Unknown";
  }
  if ((game != NULL) && IsFocusedPiece(game, player)) {
    return "You";
  }
  if ((game != NULL) && IsLocalPlayerPiece(game, player)) {
    return "You (auto)";
  }
  if (player->name[0] != '\0') {
    return player->name;
  }

  snprintf(fallback_name, sizeof(fallback_name), "%s %u", player->is_bot ? "Bot" : "Player",
           player->player_id);
  return fallback_name;
}

typedef enum PlayerThreatState {
  PLAYER_THREAT_NONE = 0,
  PLAYER_THREAT_PREY,
  PLAYER_THREAT_DANGER,
} PlayerThreatState;

typedef struct InspectCandidate {
  uint32_t player_id;
  float distance_sqr;
} InspectCandidate;

static bool IsConnectionOverlayOpen(const Game* game);
static bool IsOverlayBlockingGameplay(const Game* game);

static const float kInspectRadius = 520.0f;
static const float kInspectOpenRate = 8.5f;
static const float kInspectCloseRate = 6.0f;

static int CompareInspectCandidates(const void* left, const void* right) {
  const InspectCandidate* lhs = left;
  const InspectCandidate* rhs = right;

  if (lhs->distance_sqr < rhs->distance_sqr) {
    return -1;
  }
  if (lhs->distance_sqr > rhs->distance_sqr) {
    return 1;
  }

  return 0;
}

static const ShroomPlayerState* FindPlayerById(const Game* game, uint32_t player_id);

static bool IsLocalPlayerPiece(const Game* game, const ShroomPlayerState* player) {
  if ((game == NULL) || (player == NULL) || (game->local_player == NULL) || !player->alive) {
    return false;
  }
  return player->player_id == game->local_player->player_id;
}

static bool IsFocusedPiece(const Game* game, const ShroomPlayerState* player) {
  if (!IsLocalPlayerPiece(game, player)) {
    return false;
  }
  if (game->focused_piece_entity_id == 0) {
    return player == game->local_player;
  }
  return player->entity_id == game->focused_piece_entity_id;
}

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

static PlayerThreatState GetThreatState(const ShroomWorldState* world,
                                        const ShroomPlayerState* local_player,
                                        const ShroomPlayerState* other_player) {
  if ((local_player == NULL) || (other_player == NULL) || (local_player == other_player) ||
      !local_player->alive || !other_player->alive || (world == NULL)) {
    return PLAYER_THREAT_NONE;
  }

  if (local_player->mass >= (other_player->mass * ShroomGetConsumeMassAdvantageAtPosition(
                                                      world, other_player->position))) {
    return PLAYER_THREAT_PREY;
  }
  if (other_player->mass >= (local_player->mass * ShroomGetConsumeMassAdvantageAtPosition(
                                                      world, local_player->position))) {
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

static bool IsDecayMassActive(const ShroomWorldState* world, const ShroomPlayerState* player) {
  if ((player == NULL) || !player->alive) {
    return false;
  }
  const ShroomZone zone = ShroomGetZoneAtPosition(world, player->position);
  const float decay_threshold = zone == SHROOM_ZONE_CENTER ? (SHROOM_DEFAULT_PLAYER_MASS * 2.0f)
                                : zone == SHROOM_ZONE_MID  ? SHROOM_DECAY_MASS_THRESHOLD
                                                           : SHROOM_MAX_PLAYER_MASS;
  return player->mass > decay_threshold;
}

int GetLocalPlayerRank(const Game* game, const LeaderboardEntry* leaderboard,
                       size_t leaderboard_count) {
  for (size_t index = 0; index < leaderboard_count; ++index) {
    if (&game->world.players[leaderboard[index].index] == game->local_player) {
      return (int)index + 1;
    }
  }

  return 0;
}

static int GetPlayerRank(const Game* game, const ShroomPlayerState* target_player) {
  LeaderboardEntry leaderboard[SHROOM_MAX_PLAYERS];
  size_t leaderboard_count = 0;

  if ((game == NULL) || (target_player == NULL)) {
    return 0;
  }

  BuildLeaderboard(game, leaderboard, &leaderboard_count);
  for (size_t index = 0; index < leaderboard_count; ++index) {
    if (&game->world.players[leaderboard[index].index] == target_player) {
      return (int)index + 1;
    }
  }

  return 0;
}

static const ShroomPlayerState* FindPlayerById(const Game* game, uint32_t player_id) {
  if ((game == NULL) || (player_id == 0)) {
    return NULL;
  }

  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];

    if (player->player_id == player_id) {
      return player;
    }
  }

  return NULL;
}

static const char* GetThreatLabel(PlayerThreatState state) {
  switch (state) {
  case PLAYER_THREAT_PREY:
    return "Consumable";
  case PLAYER_THREAT_DANGER:
    return "Danger";
  case PLAYER_THREAT_NONE:
  default:
    return "Even Match";
  }
}

static int GatherInspectCandidates(const Game* game, InspectCandidate* candidates, int capacity) {
  int count = 0;

  if ((game == NULL) || (game->local_player == NULL) || !game->local_player->alive ||
      (candidates == NULL) || (capacity <= 0)) {
    return 0;
  }

  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    const float distance_sqr = ShroomDistanceSqr(game->local_player->position, player->position);

    if (!player->alive || IsLocalPlayerPiece(game, player)) {
      continue;
    }
    if (distance_sqr > (kInspectRadius * kInspectRadius)) {
      continue;
    }
    if (count >= capacity) {
      break;
    }

    candidates[count++] =
        (InspectCandidate){.player_id = player->player_id, .distance_sqr = distance_sqr};
  }

  if (count > 1) {
    qsort(candidates, (size_t)count, sizeof(candidates[0]), CompareInspectCandidates);
  }

  return count;
}

static const ShroomPlayerState* GetSelectedInspectTarget(const Game* game) {
  InspectCandidate candidates[SHROOM_MAX_PLAYERS];
  int selected_index;
  int candidate_count;

  candidate_count = GatherInspectCandidates(game, candidates, SHROOM_MAX_PLAYERS);
  if (candidate_count <= 0) {
    return NULL;
  }

  selected_index = game->selected_inspect_index;
  if (selected_index < 0) {
    selected_index = 0;
  }
  if (selected_index >= candidate_count) {
    selected_index = candidate_count - 1;
  }

  return FindPlayerById(game, candidates[selected_index].player_id);
}

static void UpdateInspectOverlay(Game* game, float delta_time) {
  InspectCandidate candidates[SHROOM_MAX_PLAYERS];
  const ShroomPlayerState* target_player;
  const bool can_show_overlay = (game != NULL) && !game->leaderboard_overlay_open &&
                                !game->menu_overlay_open && !game->leave_confirmation_open &&
                                !IsConnectionOverlayOpen(game);
  const bool inspect_held = can_show_overlay && IsKeyDown(KEY_I);
  const float wheel_move = inspect_held ? GetMouseWheelMove() : 0.0f;
  int candidate_count;

  if (game == NULL) {
    return;
  }

  game->inspect_prompt_timer += delta_time;
  candidate_count = GatherInspectCandidates(game, candidates, SHROOM_MAX_PLAYERS);
  game->inspect_target_count = candidate_count;

  if (!inspect_held) {
    game->selected_inspect_index = 0;
  } else if (candidate_count > 0) {
    if (wheel_move > 0.0f) {
      game->selected_inspect_index = (game->selected_inspect_index + 1) % candidate_count;
    } else if (wheel_move < 0.0f) {
      game->selected_inspect_index =
          (game->selected_inspect_index + candidate_count - 1) % candidate_count;
    } else if (game->selected_inspect_index >= candidate_count) {
      game->selected_inspect_index = candidate_count - 1;
    }
  }

  target_player = candidate_count > 0
                      ? FindPlayerById(game, candidates[game->selected_inspect_index].player_id)
                      : NULL;

  if (inspect_held && (target_player != NULL)) {
    game->selected_inspect_player_id = target_player->player_id;
    game->inspect_overlay_open = true;
  } else if (!inspect_held) {
    game->inspect_overlay_open = false;
  }

  if (game->inspect_overlay_open) {
    game->inspect_overlay_progress =
        fminf(1.0f, game->inspect_overlay_progress + (delta_time * kInspectOpenRate));
  } else {
    game->inspect_overlay_progress =
        fmaxf(0.0f, game->inspect_overlay_progress - (delta_time * kInspectCloseRate));
    if (game->inspect_overlay_progress <= 0.0f) {
      game->selected_inspect_player_id = 0;
    }
  }
}

static void DrawInspectPrompt(const Game* game) {
  const ShroomPlayerState* target_player;
  float pulse;

  if ((game == NULL) || game->leaderboard_overlay_open || game->menu_overlay_open ||
      game->leave_confirmation_open || IsConnectionOverlayOpen(game)) {
    return;
  }

  pulse = 0.55f + (0.25f * (0.5f + 0.5f * sinf(game->inspect_prompt_timer * 5.5f)));

  target_player = GetSelectedInspectTarget(game);
  if (target_player == NULL) {
    return;
  }

  ShroomImGui_SetNextWindowPos((game->screen_width - 360.0f) * 0.5f, game->screen_height - 92.0f,
                               SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(360.0f, 44.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.46f + pulse * 0.12f);
  if (!ShroomImGui_Begin("Inspect Prompt", NULL,
                         SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                             SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS |
                             SHROOM_IMGUI_WINDOW_NO_SCROLLBAR)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_TextColored(
      ToImGuiColor(Fade(SKYBLUE, pulse + 0.2f)),
      TextFormat("Hold I to inspect %s", GetPlayerDisplayName(game, target_player)));
  if (game->inspect_target_count > 1) {
    ShroomImGui_SameLine();
    ShroomImGui_Text(TextFormat("%d/%d  Scroll Wheel", game->selected_inspect_index + 1,
                                game->inspect_target_count));
  }
  ShroomImGui_End();
}

static void DrawPlayerName(const Game* game, const ShroomPlayerState* player, Vector2 position) {
  const char* player_name = GetPlayerDisplayName(game, player);
  const int font_size = 16;
  const int text_width = MeasureText(player_name, font_size);
  const Color text_color = player == game->local_player ? RAYWHITE : Fade(RAYWHITE, 0.92f);
  const int text_x = (int)(position.x - ((float)text_width * 0.5f));
  const int text_y = (int)(position.y - player->radius - 22.0f);

  DrawText(player_name, text_x + 1, text_y + 1, font_size, Fade(BLACK, 0.8f));
  DrawText(player_name, text_x, text_y, font_size, text_color);
}

static const char* GetInspectFlavorText(const ShroomPlayerState* selected_player) {
  if (selected_player->is_bot) {
    return "Bot profile: utility-driven colony balancing spores, prey, and danger.";
  }

  return "Player profile: live colony snapshot from the current match.";
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

    if (IsLocalPlayerPiece(game, player) || !game->net.welcome_received) {
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
  const ShroomMatchPhase previous_match_phase = game->world.match_phase;
  const ShroomMatchPhase current_match_phase = (ShroomMatchPhase)game->net.match_phase;
  const bool reset_snapshot =
      ShroomMatchFeedbackNeedsRebaseline(previous_match_phase, current_match_phase);

  game->world.tick = game->net.last_snapshot_tick;
  game->world.match_phase = current_match_phase;
  game->world.match_time_remaining = game->net.match_time_remaining;
  game->world.player_count = game->net.snapshot_player_count;
  game->world.spore_count = game->net.spore_count;
  game->world.powerup_count = game->net.powerup_count;
  game->local_player = NULL;

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
        .speed_powerup_timer =
            (snapshot_player->effect_flags & SHROOM_POWERUP_EFFECT_SPEED) != 0u ? 1.0f : 0.0f,
        .shield_powerup_timer =
            (snapshot_player->effect_flags & SHROOM_POWERUP_EFFECT_SHIELD) != 0u ? 1.0f : 0.0f,
        .magnet_powerup_timer =
            (snapshot_player->effect_flags & SHROOM_POWERUP_EFFECT_MAGNET) != 0u ? 1.0f : 0.0f,
        .decay_immune_powerup_timer =
            (snapshot_player->effect_flags & SHROOM_POWERUP_EFFECT_DECAY_IMMUNE) != 0u ? 1.0f
                                                                                       : 0.0f,
    };
    snprintf(player->name, sizeof(player->name), "%s", snapshot_player->name);

    if (player->player_id == game->net.player_id) {
      /* Prefer the piece that matches our original entity_id (the primary).
       * This ensures local_player always points to the primary piece so that
       * player_id-based lookups across all pieces remain consistent. */
      if ((game->local_player == NULL) || (player->entity_id == game->net.entity_id)) {
        game->local_player = player;
      }
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

  for (index = 0; index < game->net.powerup_count; ++index) {
    const ShroomSnapshotPowerupState* snapshot_powerup = &game->net.snapshot_powerups[index];
    ShroomPowerupState* powerup = &game->world.powerups[index];

    *powerup = (ShroomPowerupState){
        .entity_id = snapshot_powerup->entity_id,
        .position = {snapshot_powerup->position_x, snapshot_powerup->position_y},
        .type = (ShroomPowerupType)snapshot_powerup->type,
        .active = snapshot_powerup->active != 0,
    };
  }

  for (; index < SHROOM_MAX_POWERUPS; ++index) {
    game->world.powerups[index].active = false;
  }

  for (index = 0; index < SHROOM_MATCH_PODIUM_COUNT; ++index) {
    game->world.podium_player_ids[index] = game->net.podium_player_ids[index];
    game->world.podium_masses[index] = game->net.podium_masses[index];
  }

  if (reset_snapshot) {
    RebaselineMatchPresentation(game);
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

static ClientMushroomSpecies GetPlayerSpecies(const Game* game, const ShroomPlayerState* player) {
  if (IsLocalPlayerPiece(game, player)) {
    return game->settings.mushroom_species;
  }
  return (ClientMushroomSpecies)(player->player_id % CLIENT_MUSHROOM_COUNT);
}

static const ShroomMushroomSpeciesEntry* FindSpeciesEntry(const Game* game,
                                                          ClientMushroomSpecies species) {
  if ((game == NULL) || !game->net.mushroom_species_catalog_received) {
    return NULL;
  }
  for (uint8_t index = 0; index < game->net.mushroom_species_count; ++index) {
    if (game->net.mushroom_species[index].species_id == (uint8_t)species) {
      return &game->net.mushroom_species[index];
    }
  }
  return NULL;
}

static Color ColorFromRgba(uint32_t rgba) {
  return (Color){(unsigned char)((rgba >> 24u) & 0xFFu), (unsigned char)((rgba >> 16u) & 0xFFu),
                 (unsigned char)((rgba >> 8u) & 0xFFu), (unsigned char)(rgba & 0xFFu)};
}

static Color GetSpeciesCapColor(const Game* game, ClientMushroomSpecies species, Color fallback) {
  const ShroomMushroomSpeciesEntry* entry = FindSpeciesEntry(game, species);

  if ((entry != NULL) && (entry->cap_color_rgba != 0u)) {
    return ColorFromRgba(entry->cap_color_rgba);
  }

  switch (species) {
  case CLIENT_MUSHROOM_AMANITA:
    return (Color){214, 48, 58, 255};
  case CLIENT_MUSHROOM_CHANTERELLE:
    return (Color){240, 173, 58, 255};
  case CLIENT_MUSHROOM_MOREL:
    return (Color){160, 118, 72, 255};
  case CLIENT_MUSHROOM_SHIITAKE:
    return (Color){126, 82, 48, 255};
  case CLIENT_MUSHROOM_OYSTER:
    return (Color){210, 188, 178, 255};
  case CLIENT_MUSHROOM_ENOKI:
    return (Color){236, 226, 184, 255};
  case CLIENT_MUSHROOM_PORTOBELLO:
    return (Color){96, 58, 42, 255};
  case CLIENT_MUSHROOM_LIONS_MANE:
    return (Color){240, 234, 204, 255};
  case CLIENT_MUSHROOM_REISHI:
    return (Color){178, 54, 38, 255};
  case CLIENT_MUSHROOM_BLEWIT:
    return (Color){132, 92, 174, 255};
  case CLIENT_MUSHROOM_COUNT:
  default:
    return fallback;
  }
}

static void DrawSpeciesPattern(const Game* game, ClientMushroomSpecies species, Vector2 position,
                               float radius) {
  const ShroomMushroomSpeciesEntry* entry = FindSpeciesEntry(game, species);
  const uint8_t pattern_id = entry != NULL ? entry->pattern_id : (uint8_t)species;

  switch (pattern_id) {
  case CLIENT_MUSHROOM_MOREL:
    for (int i = -2; i <= 2; ++i) {
      DrawLineEx((Vector2){position.x + i * radius * 0.22f, position.y - radius * 0.58f},
                 (Vector2){position.x + i * radius * 0.08f, position.y + radius * 0.48f}, 2.0f,
                 Fade((Color){70, 48, 34, 255}, 0.42f));
      DrawLineEx((Vector2){position.x - radius * 0.56f, position.y + i * radius * 0.18f},
                 (Vector2){position.x + radius * 0.56f, position.y - i * radius * 0.08f}, 1.6f,
                 Fade((Color){70, 48, 34, 255}, 0.34f));
    }
    break;
  case CLIENT_MUSHROOM_OYSTER:
  case CLIENT_MUSHROOM_REISHI:
    for (int i = 0; i < 4; ++i) {
      DrawRing(position, radius * (0.34f + i * 0.12f), radius * (0.36f + i * 0.12f), 205.0f, 338.0f,
               20, Fade(RAYWHITE, 0.20f));
    }
    break;
  case CLIENT_MUSHROOM_LIONS_MANE:
    for (int i = 0; i < 11; ++i) {
      const float angle = ((float)i / 11.0f) * 2.0f * PI;
      DrawCircleV((Vector2){position.x + cosf(angle) * radius * 0.42f,
                            position.y + sinf(angle) * radius * 0.38f},
                  radius * 0.19f, Fade(RAYWHITE, 0.34f));
    }
    break;
  case CLIENT_MUSHROOM_ENOKI:
    for (int i = -2; i <= 2; ++i) {
      DrawLineEx((Vector2){position.x + i * radius * 0.16f, position.y + radius * 0.08f},
                 (Vector2){position.x + i * radius * 0.10f, position.y + radius * 0.72f}, 2.0f,
                 Fade((Color){255, 248, 220, 255}, 0.36f));
    }
    break;
  default:
    for (int i = 0; i < 5; i++) {
      float angle = (i / 5.0f) * 2.0f * PI;
      float spot_radius = radius * 0.18f;
      float spot_distance = radius * 0.48f;
      DrawCircle(position.x + cosf(angle) * spot_distance, position.y + sinf(angle) * spot_distance,
                 spot_radius, Fade((Color){255, 255, 255, 255}, 0.56f));
    }
    break;
  }
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

void BuildLeaderboard(const Game* game, LeaderboardEntry* entries, size_t* entry_count) {
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
         game->leave_confirmation_open || IsDeathCutsceneOpen(game) ||
         IsConnectionOverlayOpen(game);
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

static void DrawArenaZones(const ShroomWorldState* world, Rectangle view_bounds) {
  const Vector2 center = {world->width * 0.5f, world->height * 0.5f};
  const Rectangle visible_world = ClampRectToWorld(view_bounds, world);
  size_t center_player_count = 0;
  float time = GetTime();

  for (size_t index = 0; index < world->player_count; ++index) {
    if (world->players[index].alive &&
        ShroomGetZoneAtPosition(world, world->players[index].position) == SHROOM_ZONE_CENTER) {
      center_player_count += 1;
    }
  }

  // Outer zone - draw only the visible world slice to avoid full-world overdraw.
  DrawRectangleRec(visible_world, Fade((Color){25, 35, 30, 255}, 0.95f));

  // Large, visible fungal texture pattern to outer zone
  for (int i = 0; i < 16; i++) {
    float x = (i * 235.5f) + sinf(i * 0.7f + time * 0.08f) * 110.0f;
    float y = (i * 173.3f) + cosf(i * 0.5f + time * 0.1f) * 90.0f;
    if (x < world->width && y < world->height && x > 0 && y > 0 &&
        CircleIntersectsRect((Vector2){x, y}, 44.0f, view_bounds)) {
      float size = 30.0f + sinf(i + time * 0.16f) * 10.0f;
      DrawCircle(x, y, size, Fade((Color){50, 80, 50, 255}, 0.28f));
    }
  }

  // Mid zone - vibrant mycelium network with strong visibility
  if (CircleIntersectsRect(center, SHROOM_ZONE_MID_RADIUS, view_bounds)) {
    DrawCircleV(center, SHROOM_ZONE_MID_RADIUS, Fade((Color){45, 75, 50, 255}, 0.85f));
  }

  // Prominent mycelium-like patterns. Keep these static; rotating world
  // decoration reads like the camera is orbiting in Offline Practice.
  for (int i = 0; i < 8; i++) {
    float angle = (i / 8.0f) * 2.0f * PI;
    float radius = SHROOM_ZONE_MID_RADIUS * 0.7f;
    float x = center.x + cosf(angle) * radius;
    float y = center.y + sinf(angle) * radius;
    float size = 42.0f + sinf(i * 2.0f + time * 0.22f) * 12.0f;
    if (CircleIntersectsRect((Vector2){x, y}, size, view_bounds)) {
      DrawCircle(x, y, size, Fade((Color){80, 130, 80, 255}, 0.32f));
    }
  }

  // Center zone - bright, glowing prime fungal growth area
  if (CircleIntersectsRect(center, SHROOM_ZONE_CENTER_RADIUS, view_bounds)) {
    DrawCircleV(center, SHROOM_ZONE_CENTER_RADIUS, Fade((Color){70, 110, 55, 255}, 0.9f));
  }

  // Strong glowing center effect with pulsing
  float pulse = 0.5f + 0.5f * sinf(time * 2.0f);
  if (CircleIntersectsRect(center, SHROOM_ZONE_CENTER_RADIUS * 0.6f, view_bounds)) {
    DrawCircleGradient((int)center.x, (int)center.y, SHROOM_ZONE_CENTER_RADIUS * 0.6f,
                       Fade((Color){150, 220, 100, 255}, 0.5f + pulse * 0.2f),
                       Fade((Color){70, 110, 55, 255}, 0.0f));
  }

  // Add static radial glow lines; animated rotation was mistaken for camera spin.
  for (int i = 0; i < 6; i++) {
    float angle = (i / 6.0f) * 2.0f * PI;
    float inner_radius = SHROOM_ZONE_CENTER_RADIUS * 0.3f;
    float outer_radius = SHROOM_ZONE_CENTER_RADIUS * 0.9f;
    Vector2 start = {center.x + cosf(angle) * inner_radius, center.y + sinf(angle) * inner_radius};
    Vector2 end = {center.x + cosf(angle) * outer_radius, center.y + sinf(angle) * outer_radius};
    if (CircleIntersectsRect(center, outer_radius, view_bounds)) {
      DrawLineEx(start, end, 5.0f, Fade((Color){180, 240, 120, 255}, 0.24f + pulse * 0.12f));
    }
  }

  if (center_player_count >= 3u &&
      CircleIntersectsRect(center, SHROOM_ZONE_CENTER_RADIUS, view_bounds)) {
    DrawCircleV(center, SHROOM_ZONE_CENTER_RADIUS * 0.78f, Fade(LIME, 0.25f));
    DrawCircleLines((int)center.x, (int)center.y, SHROOM_ZONE_CENTER_RADIUS * 0.86f,
                    Fade(LIME, 0.7f));
    DrawCircleLines((int)center.x, (int)center.y, SHROOM_ZONE_CENTER_RADIUS * 0.92f,
                    Fade((Color){200, 255, 150, 255}, 0.5f));
  }

  // Zone boundaries with strong, visible fungal edge effects
  if (CircleIntersectsRect(center, SHROOM_ZONE_MID_RADIUS, view_bounds)) {
    DrawCircleLines((int)center.x, (int)center.y, SHROOM_ZONE_MID_RADIUS,
                    Fade((Color){100, 160, 100, 255}, 0.6f));
    DrawCircleLines((int)center.x, (int)center.y, SHROOM_ZONE_MID_RADIUS - 3.0f,
                    Fade((Color){120, 180, 120, 255}, 0.5f));
    DrawCircleLines((int)center.x, (int)center.y, SHROOM_ZONE_MID_RADIUS - 6.0f,
                    Fade((Color){140, 200, 140, 255}, 0.4f));
  }

  if (CircleIntersectsRect(center, SHROOM_ZONE_CENTER_RADIUS, view_bounds)) {
    DrawCircleLines((int)center.x, (int)center.y, SHROOM_ZONE_CENTER_RADIUS,
                    Fade((Color){180, 240, 120, 255}, 0.7f));
    DrawCircleLines((int)center.x, (int)center.y, SHROOM_ZONE_CENTER_RADIUS - 3.0f,
                    Fade((Color){200, 255, 150, 255}, 0.6f));
    DrawCircleLines((int)center.x, (int)center.y, SHROOM_ZONE_CENTER_RADIUS - 6.0f,
                    Fade((Color){220, 255, 180, 255}, 0.5f));
  }
}

static void DrawBoundaryConsumeWarning(const Game* game, Rectangle view_bounds) {
  const ShroomWorldState* world;
  const ShroomPlayerState* local_player;
  Rectangle visible_world;
  float intensity;
  Color warning_color;

  if ((game == NULL) || (game->local_player == NULL) || !game->local_player->alive) {
    return;
  }

  world = &game->world;
  local_player = game->local_player;
  visible_world = ClampRectToWorld(view_bounds, world);
  intensity = GetBoundaryConsumeWarningIntensity(world, local_player->position);
  if (intensity <= 0.0f) {
    return;
  }

  warning_color = Fade((Color){255, 88, 62, 255}, 0.10f + (0.24f * intensity));
  if (visible_world.x <= SHROOM_BOUNDARY_CONSUME_MARGIN) {
    DrawRectangleRec(
        (Rectangle){0.0f, visible_world.y, SHROOM_BOUNDARY_CONSUME_MARGIN, visible_world.height},
        warning_color);
  }
  if ((visible_world.x + visible_world.width) >= (world->width - SHROOM_BOUNDARY_CONSUME_MARGIN)) {
    DrawRectangleRec((Rectangle){world->width - SHROOM_BOUNDARY_CONSUME_MARGIN, visible_world.y,
                                 SHROOM_BOUNDARY_CONSUME_MARGIN, visible_world.height},
                     warning_color);
  }
  if (visible_world.y <= SHROOM_BOUNDARY_CONSUME_MARGIN) {
    DrawRectangleRec(
        (Rectangle){visible_world.x, 0.0f, visible_world.width, SHROOM_BOUNDARY_CONSUME_MARGIN},
        warning_color);
  }
  if ((visible_world.y + visible_world.height) >=
      (world->height - SHROOM_BOUNDARY_CONSUME_MARGIN)) {
    DrawRectangleRec((Rectangle){visible_world.x, world->height - SHROOM_BOUNDARY_CONSUME_MARGIN,
                                 visible_world.width, SHROOM_BOUNDARY_CONSUME_MARGIN},
                     warning_color);
  }
}

static void DrawSpores(const ShroomWorldState* world, Rectangle view_bounds) {
  float time = GetTime();

  for (size_t index = 0; index < world->spore_count; ++index) {
    const ShroomSporeState* spore = &world->spores[index];

    const Vector2 position = {spore->position.x, spore->position.y};
    if (!spore->active || !CircleIntersectsRect(position, 18.0f, view_bounds)) {
      continue;
    }
    float pulse = 0.7f + 0.3f * sinf(time * 3.0f + (float)index * 0.5f);

    // Visible glow without excessive per-spore overdraw.
    DrawCircleV(position, 13.0f * pulse, Fade((Color){255, 230, 100, 255}, 0.28f));

    // Main spore body with strong gradient
    DrawCircleGradient((int)position.x, (int)position.y, 7.0f, (Color){255, 240, 140, 255},
                       Fade((Color){255, 200, 80, 255}, 0.9f));

    // Bright highlight for 3D effect
    DrawCircleV((Vector2){position.x - 2.0f, position.y - 2.0f}, 3.0f,
                Fade((Color){255, 255, 220, 255}, 0.95f));

    if ((index % 9u) == 0u) {
      DrawCircleV(position, 2.0f, Fade((Color){255, 255, 255, 255}, 0.55f * pulse));
    }
  }
}

static Color GetPowerupColor(ShroomPowerupType type) {
  switch (type) {
  case SHROOM_POWERUP_SPEED:
    return SKYBLUE;
  case SHROOM_POWERUP_SHIELD:
    return VIOLET;
  case SHROOM_POWERUP_MAGNET:
    return LIME;
  case SHROOM_POWERUP_DECAY_IMMUNE:
    return GOLD;
  default:
    return RAYWHITE;
  }
}

static void DrawPowerups(const ShroomWorldState* world, Rectangle view_bounds) {
  float time = GetTime();

  for (size_t index = 0; index < world->powerup_count; ++index) {
    const ShroomPowerupState* powerup = &world->powerups[index];
    const Color color = GetPowerupColor(powerup->type);

    const Vector2 position = {powerup->position.x, powerup->position.y};
    if (!powerup->active ||
        !CircleIntersectsRect(position, SHROOM_POWERUP_RADIUS + 34.0f, view_bounds)) {
      continue;
    }
    float pulse = sinf((float)index * 0.5f + time * 4.0f) * 0.4f + 0.8f;
    float rotation = (float)index;

    // Large outer glow with restrained overdraw.
    DrawCircleV(position, SHROOM_POWERUP_RADIUS + 24.0f, Fade(color, 0.20f * pulse));
    DrawCircleV(position, SHROOM_POWERUP_RADIUS + 14.0f, Fade(color, 0.34f * pulse));

    // Rotating ring effects
    for (int i = 0; i < 4; i++) {
      float angle = rotation + (i / 4.0f) * 2.0f * PI;
      float radius = SHROOM_POWERUP_RADIUS + 16.0f;
      float x = position.x + cosf(angle) * radius;
      float y = position.y + sinf(angle) * radius;
      DrawCircle(x, y, 4.0f, Fade(color, 0.6f * pulse));
    }

    // Main powerup body with strong visibility
    DrawCircleV(position, SHROOM_POWERUP_RADIUS + 2.0f, Fade(BLACK, 0.5f));
    DrawCircleV(position, SHROOM_POWERUP_RADIUS, Fade(color, 0.95f));

    // Bright inner highlight
    DrawCircleV((Vector2){position.x - 3.0f, position.y - 3.0f}, SHROOM_POWERUP_RADIUS * 0.6f,
                Fade((Color){255, 255, 255, 255}, 0.7f));

    // Animated rings with strong visibility
    DrawCircleLines((int)position.x, (int)position.y, SHROOM_POWERUP_RADIUS + 6.0f + pulse * 3.0f,
                    Fade(RAYWHITE, 0.8f));
    DrawCircleLines((int)position.x, (int)position.y, SHROOM_POWERUP_RADIUS + 12.0f + pulse * 4.0f,
                    Fade(color, 0.6f));
  }
}

static Color GetSpawnProtectionRingColor(float remaining_seconds) {
  const float alpha =
      Clamp(0.22f + (remaining_seconds / SHROOM_SPLIT_PROTECTION_SECONDS) * 0.58f, 0.22f, 0.80f);
  return Fade((Color){92, 225, 255, 255}, alpha);
}

static Color GetShieldShellColor(float remaining_seconds) {
  const float alpha =
      Clamp(0.42f + (remaining_seconds / SHROOM_POWERUP_SHIELD_SECONDS) * 0.48f, 0.42f, 0.90f);
  return Fade((Color){255, 72, 218, 255}, alpha);
}

static void DrawSpawnProtectionRing(Vector2 position, float radius, float remaining_seconds) {
  const float pulse = 0.65f + (0.35f * sinf((float)GetTime() * 9.0f));
  const Color color = GetSpawnProtectionRingColor(remaining_seconds);

  DrawCircleLines((int)position.x, (int)position.y, radius + 10.0f + (pulse * 3.0f), color);
  DrawCircleLines((int)position.x, (int)position.y, radius + 14.0f + (pulse * 5.0f),
                  Fade(color, 0.42f));
}

static void DrawShieldShell(Vector2 position, float radius, float remaining_seconds) {
  const float shell_radius = radius + 22.0f;
  const Color color = GetShieldShellColor(remaining_seconds);
  Vector2 points[6];

  for (int index = 0; index < 6; ++index) {
    const float angle = -1.5707963f + ((float)index * 1.0471976f);
    points[index] =
        (Vector2){position.x + cosf(angle) * shell_radius, position.y + sinf(angle) * shell_radius};
  }
  for (int index = 0; index < 6; ++index) {
    DrawLineEx(points[index], points[(index + 1) % 6], 3.0f, color);
  }
  for (int index = 0; index < 6; ++index) {
    DrawCircleV(points[index], 3.0f, Fade(color, 0.85f));
  }
}

static void DrawPlayers(const Game* game, Rectangle view_bounds) {
  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    const ShroomVec2 render_position = game->render_positions[index];
    const Vector2 position = {render_position.x, render_position.y};
    const bool is_local = IsLocalPlayerPiece(game, player);
    const bool is_focused = is_local && IsFocusedPiece(game, player);

    if (!player->alive || !CircleIntersectsRect(position, player->radius + 80.0f, view_bounds)) {
      continue;
    }

    {
      const ClientMushroomSpecies species = GetPlayerSpecies(game, player);
      const Color fallback_fill = GetPlayerFillColor(game, player);
      const Color fill = game->settings.palette_preset == CLIENT_PALETTE_HIGH_CONTRAST
                             ? fallback_fill
                             : GetSpeciesCapColor(game, species, fallback_fill);
      const PlayerThreatState threat_state =
          GetThreatState(&game->world, game->local_player, player);
      const Color threat_outline = GetThreatOutlineColor(threat_state);
      const float decay_pulse =
          0.45f + (0.35f * (0.5f + (0.5f * sinf(game->inspect_prompt_timer * 5.0f))));
      const ShroomPlayerRenderLodTier lod = ShroomPlayerRenderLodForRadius(player->radius);
      const int text_shadow_passes = ShroomPlayerRenderLodTextShadowPasses(lod);
      const bool is_very_large = player->radius >= 140.0f;

      // Strong shadow underneath for depth (skip for very large players to reduce overdraw)
      if (!is_very_large) {
        DrawCircleV(position, player->radius + 5.0f, Fade((Color){20, 15, 10, 255}, 0.7f));
      }

      // Mushroom cap base (darker underside) - very visible
      DrawCircleV(position, player->radius + 3.0f, Fade((Color){50, 35, 25, 255}, 0.8f));

      // Main mushroom cap with strong color
      DrawCircleV(position, player->radius, fill);

      if (is_local && ShroomPlayerRenderLodShouldDrawInnerHighlight(lod)) {
        DrawCircleV(position, player->radius * 0.78f, Fade((Color){255, 230, 120, 255}, 0.18f));
      }

      // Large, bright mushroom cap highlight (top lighting)
      if (ShroomPlayerRenderLodShouldDrawPrimaryTopLight(lod)) {
        DrawCircleV(
            (Vector2){position.x - player->radius * 0.25f, position.y - player->radius * 0.25f},
            player->radius * 0.7f, Fade((Color){255, 255, 255, 255}, 0.35f));
      }

      // Secondary highlight for more depth
      if (ShroomPlayerRenderLodShouldDrawSecondaryHighlight(lod)) {
        DrawCircleV(
            (Vector2){position.x - player->radius * 0.15f, position.y - player->radius * 0.35f},
            player->radius * 0.4f, Fade((Color){255, 255, 255, 255}, 0.25f));
      }

      // Large, visible mushroom species markings based on size.
      if ((player->radius > 12.0f) && ShroomPlayerRenderLodShouldDrawSpeciesPattern(lod)) {
        DrawSpeciesPattern(game, species, position, player->radius);
      }

      char mass_text[32];
      snprintf(mass_text, sizeof(mass_text), "%.0f", player->mass);

      int font_size = (int)(player->radius * 0.55f);
      if (font_size < 8)
        font_size = 8;
      // Cap text size more aggressively for very large players to reduce rendering cost
      if (is_very_large) {
        if (font_size > 24)
          font_size = 24;
      } else {
        if (font_size > 34)
          font_size = 34;
      }

      const int text_width = MeasureText(mass_text, font_size);
      const int text_x = (int)(position.x - text_width * 0.5f);
      const int text_y = (int)(position.y - font_size * 0.5f);
      const Rectangle text_badge = {(float)text_x - 5.0f, (float)text_y - 2.0f,
                                    (float)text_width + 10.0f, (float)font_size + 5.0f};
      const Color label_color =
          is_local ? (Color){255, 250, 205, 255} : (Color){241, 236, 218, 255};
      const float label_alpha = is_local ? 1.0f : 0.86f;
      const float badge_alpha = is_local ? 0.72f : 0.46f;

      DrawRectangleRounded(text_badge, 0.45f, 8, Fade((Color){22, 14, 9, 255}, badge_alpha));
      DrawRectangleRoundedLines(text_badge, 0.45f, 8,
                                Fade((Color){255, 216, 118, 255}, is_local ? 0.72f : 0.32f));

      /* Text shadow: slice through the four legacy offset passes per the
       * player's LOD tier so a large player pays at most `text_shadow_passes`
       * shadow fills instead of four. Order matches the hard-coded offsets
       * that preceded this block (right, left, down, up) so FULL is unchanged. */
      static const Vector2 shadow_offsets[4] = {{2, 0}, {-2, 0}, {0, 2}, {0, -2}};
      for (int pass = 0; pass < text_shadow_passes; ++pass) {
        DrawText(mass_text, text_x + (int)shadow_offsets[pass].x,
                 text_y + (int)shadow_offsets[pass].y, font_size,
                 Fade((Color){12, 8, 5, 255}, 0.9f));
      }
      DrawText(mass_text, text_x, text_y, font_size, Fade(label_color, label_alpha));

      // Outline rings (reduce for very large players)
      DrawCircleLines((int)position.x, (int)position.y, player->radius + 3.0f, Fade(BLACK, 0.55f));
      if (!is_local && !is_very_large) {
        DrawCircleLines((int)position.x, (int)position.y, player->radius + 6.0f, threat_outline);
      } else if (!is_focused) {
        /* Unfocused local piece: yellow ring to distinguish from enemy and focused piece. */
        DrawCircleLines((int)position.x, (int)position.y, player->radius + 6.0f,
                        Fade(YELLOW, 0.70f));
      }
      if (is_focused && !is_very_large) {
        DrawCircleLines((int)position.x, (int)position.y, player->radius + 8.0f,
                        Fade((Color){255, 236, 140, 255}, 0.95f));
        DrawCircleLines((int)position.x, (int)position.y, player->radius + 12.0f,
                        Fade((Color){255, 255, 220, 255}, 0.48f));
      }
      if (player->spawn_protection_timer > 0.0f) {
        DrawSpawnProtectionRing(position, player->radius, player->spawn_protection_timer);
      }
      if (player->shield_powerup_timer > 0.0f) {
        DrawShieldShell(position, player->radius, player->shield_powerup_timer);
      }
      if (IsDecayMassActive(&game->world, player) && !is_very_large) {
        DrawCircleLines((int)position.x, (int)position.y, player->radius + 11.0f,
                        Fade(RED, decay_pulse));
      }
      if (!is_local && IsPositionInBoundaryConsumeZone(&game->world, player->position) &&
          ShroomPlayerCanConsume(&game->world, player, game->local_player) && !is_very_large) {
        const float halo_pulse = 0.55f + (0.30f * sinf((float)GetTime() * 7.0f));
        DrawCircleLines((int)position.x, (int)position.y, player->radius + 14.0f,
                        Fade((Color){255, 76, 48, 255}, halo_pulse));
        DrawCircleLines((int)position.x, (int)position.y, player->radius + 20.0f,
                        Fade((Color){255, 150, 68, 255}, 0.26f + (0.18f * halo_pulse)));
      }
      /* Hold-to-split: charge arc + large low-latency launch direction arrow. */
      if (is_focused && (game->split_hold_timer > 0.0f) && !is_very_large) {
        const float progress =
            Clamp(game->split_hold_timer / SHROOM_SPLIT_HOLD_SECONDS, 0.0f, 1.0f);
        const float pulse = 0.72f + (0.28f * sinf((float)GetTime() * 18.0f));
        const Color arc_color = (progress >= 1.0f) ? RAYWHITE : (Color){92, 220, 255, 255};

        DrawRing(position, player->radius + 14.0f, player->radius + 18.0f, -90.0f,
                 -90.0f + (progress * 360.0f), 36, Fade(arc_color, 0.85f));

        if (ShroomVec2LengthSqr(game->split_aim_visual_direction) > 0.01f) {
          const float arrow_len = player->radius + 70.0f + (progress * 54.0f) + (pulse * 10.0f);
          const float shaft_width = 8.0f + (progress * 4.0f);
          const float head_len = 38.0f + (progress * 10.0f);
          const float head_half_width = 24.0f + (progress * 8.0f);
          const Vector2 dir = {game->split_aim_visual_direction.x,
                               game->split_aim_visual_direction.y};
          const Vector2 start = {position.x + dir.x * (player->radius + 18.0f),
                                 position.y + dir.y * (player->radius + 18.0f)};
          const Vector2 tip = {position.x + dir.x * arrow_len, position.y + dir.y * arrow_len};
          const Vector2 base = {tip.x - dir.x * head_len, tip.y - dir.y * head_len};
          const Vector2 left = {-dir.y * head_half_width, dir.x * head_half_width};
          const Vector2 right = {dir.y * head_half_width, -dir.x * head_half_width};
          const Color glow = Fade((Color){55, 195, 255, 255}, 0.25f + (progress * 0.18f));
          const Color arrow_fill = Fade(arc_color, 0.84f + (progress * 0.12f));

          DrawLineEx(start, base, shaft_width + 8.0f, glow);
          DrawLineEx(start, base, shaft_width, arrow_fill);
          DrawTriangle(tip, (Vector2){base.x + left.x, base.y + left.y},
                       (Vector2){base.x + right.x, base.y + right.y}, arrow_fill);
          DrawTriangleLines(tip, (Vector2){base.x + left.x, base.y + left.y},
                            (Vector2){base.x + right.x, base.y + right.y}, Fade(RAYWHITE, 0.76f));
        }
      }
    }

    DrawPlayerName(game, player, position);
  }
}

static void DrawInspectOverlay(Game* game) {
  const ShroomPlayerState* selected_player;
  PlayerThreatState threat_state;
  ShroomZone zone;
  int rank;
  const float progress = game != NULL ? game->inspect_overlay_progress : 0.0f;
  const float eased_progress = progress * progress * (3.0f - (2.0f * progress));
  const float scale = 0.96f + (0.04f * eased_progress);
  const float width = 328.0f * scale;
  const float height = 272.0f * scale;
  float pulse;

  if ((game == NULL) || (progress <= 0.01f)) {
    return;
  }

  pulse = 0.72f + (0.28f * (0.5f + 0.5f * sinf(game->inspect_prompt_timer * 6.5f)));

  selected_player = FindPlayerById(game, game->selected_inspect_player_id);
  if ((selected_player == NULL) || !selected_player->alive) {
    game->inspect_overlay_open = false;
    return;
  }

  threat_state = GetThreatState(&game->world, game->local_player, selected_player);
  zone = ShroomGetZoneAtPosition(&game->world, selected_player->position);
  rank = GetPlayerRank(game, selected_player);

  ShroomImGui_SetNextWindowPos((game->screen_width - width) * 0.5f,
                               (game->screen_height - height) * 0.5f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(width, height, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.24f + (0.46f * eased_progress));
  if (!ShroomImGui_Begin(
          "Player Intel", NULL,
          SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
              SHROOM_IMGUI_WINDOW_NO_COLLAPSE | SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS |
              SHROOM_IMGUI_WINDOW_NO_SCROLLBAR | SHROOM_IMGUI_WINDOW_NO_SCROLL_WITH_MOUSE)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_Text(GetPlayerDisplayName(game, selected_player));
  ShroomImGui_TextColored(ToImGuiColor(Fade(GetThreatOutlineColor(threat_state), pulse)),
                          GetThreatLabel(threat_state));
  if (game->inspect_target_count > 1) {
    ShroomImGui_Text(TextFormat("Target %d/%d  Scroll Wheel to cycle",
                                game->selected_inspect_index + 1, game->inspect_target_count));
  }
  ShroomImGui_Separator();
  ShroomImGui_Text(TextFormat("Mass %.0f", selected_player->mass));
  ShroomImGui_Text(TextFormat("Rank %d", rank > 0 ? rank : (int)game->world.player_count));
  ShroomImGui_Text(TextFormat("Zone %s", GetZoneLabel(zone)));
  ShroomImGui_Text(TextFormat("Type %s", selected_player->is_bot ? "Bot Colony" : "Player Colony"));
  ShroomImGui_Text(TextFormat("Player ID %u", selected_player->player_id));
  ShroomImGui_TextWrapped(GetInspectFlavorText(selected_player));
  ShroomImGui_Spacing();
  ShroomImGui_Text("Release I to dismiss.");

  ShroomImGui_End();
}

static void DrawOffscreenIndicators(const Game* game) {
  const Vector2 screen_center = {game->screen_width * 0.5f, game->screen_height * 0.5f};

  if (game->local_player == NULL) {
    return;
  }
  const float horizontal_limit = (game->screen_width * 0.5f) - 44.0f;
  const float vertical_limit = (game->screen_height * 0.5f) - 44.0f;

  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    PlayerThreatState threat_state;
    Color color;
    Vector2 world_position;
    Vector2 screen_position;
    Vector2 direction;
    Vector2 indicator_position;
    float scale_x;
    float scale_y;
    float scale;

    /* Skip dead players and the local player before any other computation. */
    if (!player->alive || (player == game->local_player)) {
      continue;
    }

    world_position = (Vector2){game->render_positions[index].x, game->render_positions[index].y};
    screen_position = GetWorldToScreen2D(world_position, game->camera);

    /* Skip players that are visible on screen. */
    if ((screen_position.x >= 0.0f) && (screen_position.x <= (float)game->screen_width) &&
        (screen_position.y >= 0.0f) && (screen_position.y <= (float)game->screen_height)) {
      continue;
    }

    threat_state = GetThreatState(&game->world, game->local_player, player);
    /* Neutral players get a dim but visible tint so all off-screen players
     * show an indicator regardless of mass relationship. */
    color = (threat_state == PLAYER_THREAT_NONE)
                ? (player->is_bot ? Fade(LIGHTGRAY, 0.55f) : Fade(RAYWHITE, 0.55f))
                : GetThreatOutlineColor(threat_state);

    direction = Vector2Subtract(screen_position, screen_center);
    if (Vector2LengthSqr(direction) <= 0.01f) {
      continue;
    }

    scale_x = horizontal_limit / fmaxf(fabsf(direction.x), 0.0001f);
    scale_y = vertical_limit / fmaxf(fabsf(direction.y), 0.0001f);
    scale = fminf(scale_x, scale_y);
    indicator_position = Vector2Add(screen_center, Vector2Scale(direction, scale));
    direction = Vector2Normalize(direction);

    /* Directional mushroom marker: cap points toward the off-screen colony. */
    {
      const Vector2 perpendicular = {-direction.y, direction.x};
      const Vector2 cap_tip = Vector2Add(indicator_position, Vector2Scale(direction, 12.0f));
      const Vector2 cap_center = Vector2Subtract(indicator_position, Vector2Scale(direction, 2.0f));
      const Vector2 cap_back = Vector2Subtract(indicator_position, Vector2Scale(direction, 12.0f));
      const Vector2 cap_left = Vector2Add(cap_back, Vector2Scale(perpendicular, 18.0f));
      const Vector2 cap_right = Vector2Subtract(cap_back, Vector2Scale(perpendicular, 18.0f));
      const Vector2 gill_left = Vector2Add(cap_back, Vector2Scale(perpendicular, 13.0f));
      const Vector2 gill_right = Vector2Subtract(cap_back, Vector2Scale(perpendicular, 13.0f));
      const Vector2 stem_base = Vector2Subtract(indicator_position, Vector2Scale(direction, 32.0f));
      const Vector2 stem_top = Vector2Subtract(indicator_position, Vector2Scale(direction, 13.0f));
      const Vector2 stem_left_base = Vector2Add(stem_base, Vector2Scale(perpendicular, 4.5f));
      const Vector2 stem_right_base = Vector2Subtract(stem_base, Vector2Scale(perpendicular, 4.5f));
      const Vector2 stem_left_top = Vector2Add(stem_top, Vector2Scale(perpendicular, 7.0f));
      const Vector2 stem_right_top = Vector2Subtract(stem_top, Vector2Scale(perpendicular, 7.0f));
      const Color spore_color =
          Fade((Color){244, 214, 126, 255}, threat_state == PLAYER_THREAT_NONE ? 0.46f : 0.78f);
      const Color stem_color = Fade((Color){236, 214, 174, 255}, 0.92f);
      const Color gill_color = Fade((Color){249, 226, 186, 255}, 0.88f);

      DrawTriangle(cap_tip, cap_left, cap_right, Fade(BLACK, 0.54f));
      DrawCircleV(cap_center, 17.0f, Fade(BLACK, 0.38f));
      DrawTriangle(stem_left_base, stem_right_base, stem_left_top, Fade(BLACK, 0.44f));
      DrawTriangle(stem_right_base, stem_right_top, stem_left_top, Fade(BLACK, 0.44f));

      DrawTriangle(stem_left_base, stem_right_base, stem_left_top, stem_color);
      DrawTriangle(stem_right_base, stem_right_top, stem_left_top, stem_color);
      DrawTriangle(cap_tip, cap_left, cap_right, Fade(color, 0.98f));
      DrawCircleV(cap_center, 11.0f, Fade(color, 0.96f));
      DrawLineEx(gill_left, gill_right, 4.0f, gill_color);
      DrawLineEx(Vector2Add(gill_left, Vector2Scale(direction, 2.0f)),
                 Vector2Add(gill_right, Vector2Scale(direction, 2.0f)), 1.5f,
                 Fade((Color){112, 76, 54, 255}, 0.34f));

      DrawCircleV(Vector2Add(cap_center, Vector2Scale(perpendicular, 6.0f)), 3.6f,
                  Fade(RAYWHITE, 0.76f));
      DrawCircleV(Vector2Subtract(cap_center, Vector2Scale(perpendicular, 5.0f)), 2.6f,
                  Fade(RAYWHITE, 0.60f));
      DrawCircleV(Vector2Add(cap_center, Vector2Scale(direction, 5.0f)), 2.4f,
                  Fade(RAYWHITE, 0.52f));

      for (int spore_index = 0; spore_index < 3; ++spore_index) {
        const float distance = 24.0f + (float)spore_index * 9.0f;
        const float side = (spore_index == 1) ? -1.0f : 1.0f;
        const Vector2 spore_position =
            Vector2Add(Vector2Subtract(indicator_position, Vector2Scale(direction, distance)),
                       Vector2Scale(perpendicular, side * (4.0f + (float)spore_index * 1.5f)));
        DrawCircleV(spore_position, 3.5f - (float)spore_index * 0.45f, spore_color);
      }
    }
  }
}

static void UpdateStatusBanners(Game* game, float delta_time) {
  ShroomZone zone;

  if (game->local_player == NULL) {
    return;
  }
  zone = ShroomGetZoneAtPosition(&game->world, game->local_player->position);
  const float moved_distance =
      sqrtf(ShroomDistanceSqr(game->local_player->position, game->previous_local_position));

  if (zone != game->current_zone) {
    QueueGameplayZoneCallout(game, zone);
    game->current_zone = zone;
  } else if (game->zone_callout_timer > 0.0f) {
    game->zone_callout_timer -= delta_time;
  }

  if ((game->previous_local_mass > (SHROOM_DEFAULT_PLAYER_MASS * 1.5f)) &&
      (game->local_player->mass <= (SHROOM_DEFAULT_PLAYER_MASS + 0.1f)) &&
      (moved_distance > 250.0f)) {
    QueueGameplayRespawnBanner(game);
  } else if (game->respawn_banner_timer > 0.0f) {
    game->respawn_banner_timer -= delta_time;
  }

  DispatchQueuedGameplayEvents(game);

  game->previous_local_mass = game->local_player->mass;
  game->previous_local_position = game->local_player->position;
}

static void DrawStatusBanners(const Game* game) {
  /* Stack banners vertically below the combat notification stream. The
   * notification panel occupies y=24..82 (24px top + 58px height) when
   * active; start banners below it with a 12px gap so they never overlap. */
  const float notification_bottom = 24.0f + 58.0f;
  const float top_y = (game->notification_count > 0u) ? notification_bottom + 12.0f : 18.0f;
  float next_y = top_y;

  if (game->zone_callout_timer > 0.0f) {
    ShroomImGui_SetNextWindowPos((game->screen_width - 420) * 0.5f, next_y,
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
    next_y += 76.0f + 12.0f;
  }

  if (game->respawn_banner_timer > 0.0f) {
    ShroomImGui_SetNextWindowPos((game->screen_width - 360) * 0.5f, next_y,
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
    next_y += 56.0f + 12.0f;
  }

  if (IsOnlineMode(game->active_mode) && (game->net.status == CLIENT_NET_CONNECTED) &&
      (game->net.rtt_sample_count > 0u) &&
      (game->net.rtt_average_ms >= SHROOM_LATENCY_WARNING_MS)) {
    const uint32_t display_rtt =
        game->net.rtt_average_ms > 9999u ? 9999u : game->net.rtt_average_ms;
    const bool unplayable = display_rtt >= SHROOM_LATENCY_UNPLAYABLE_MS;
    const Color warn_color = unplayable ? RED : ORANGE;
    ShroomImGui_SetNextWindowPos((game->screen_width - 300.0f) * 0.5f, next_y,
                                 SHROOM_IMGUI_COND_ALWAYS);
    ShroomImGui_SetNextWindowSize(300.0f, 44.0f, SHROOM_IMGUI_COND_ALWAYS);
    ShroomImGui_SetNextWindowBgAlpha(0.78f);
    if (ShroomImGui_Begin("Latency Warning", NULL,
                          SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                              SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                              SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS |
                              SHROOM_IMGUI_WINDOW_NO_SCROLLBAR)) {
      ShroomImGui_TextColored(ToImGuiColor(warn_color),
                              unplayable ? "Unplayable latency" : "High latency");
      ShroomImGui_SameLine();
      ShroomImGui_Text(TextFormat("avg RTT %ums", display_rtt));
    }
    ShroomImGui_End();
  }
}

static void UpdateCombatFeedback(Game* game) {
  LeaderboardEntry leaderboard[SHROOM_MAX_PLAYERS];
  size_t leaderboard_count = 0;
  int local_rank;

  if (game->local_player == NULL) {
    return;
  }

  BuildLeaderboard(game, leaderboard, &leaderboard_count);
  local_rank = GetLocalPlayerRank(game, leaderboard, leaderboard_count);
  if (local_rank > 0) {
    if ((game->previous_local_rank > 0) && (local_rank != game->previous_local_rank)) {
      QueueGameplayNotification(game,
                                local_rank < game->previous_local_rank ? "Rank up" : "Rank down",
                                TextFormat("Canopy rank %d/%d", local_rank, (int)leaderboard_count),
                                local_rank < game->previous_local_rank ? (Color){112, 224, 128, 255}
                                                                       : (Color){245, 184, 84, 255},
                                2.2f);
    }
    game->previous_local_rank = local_rank;
  }

  if (game->combat_feedback_cooldown > 0.0f) {
    return;
  }

  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    const PlayerThreatState threat_state = GetThreatState(&game->world, game->local_player, player);
    const float distance_sqr = ShroomDistanceSqr(game->local_player->position, player->position);
    const float close_call_radius = game->local_player->radius + player->radius + 62.0f;

    if (!player->alive || IsLocalPlayerPiece(game, player) ||
        (threat_state != PLAYER_THREAT_DANGER)) {
      continue;
    }
    if (distance_sqr <= (close_call_radius * close_call_radius)) {
      QueueGameplayNotification(
          game, "Close call",
          TextFormat("%s is big enough to consume you", GetPlayerDisplayName(game, player)),
          (Color){245, 84, 84, 255}, 2.0f);
      QueueGameplaySfx(game, SHROOM_CLIENT_SFX_WARNING, 0.74f);
      game->combat_feedback_cooldown = 4.0f;
      break;
    }
    if (distance_sqr <= (420.0f * 420.0f)) {
      QueueGameplayNotification(
          game, "Danger nearby",
          TextFormat("Avoid %s until you grow", GetPlayerDisplayName(game, player)),
          (Color){245, 184, 84, 255}, 2.0f);
      QueueGameplaySfx(game, SHROOM_CLIENT_SFX_WARNING, 0.54f);
      game->combat_feedback_cooldown = 4.0f;
      break;
    }
  }

  DispatchQueuedGameplayEvents(game);
}

static void DrawLeaderboardOverlay(Game* game, const LeaderboardEntry* leaderboard,
                                   size_t shown_count) {
  if (!game->leaderboard_overlay_open) {
    return;
  }

  DrawFungalHudPanel((Rectangle){(game->screen_width - 440.0f) * 0.5f, 120.0f, 440.0f, 280.0f},
                     (Color){170, 215, 118, 255});
  ShroomImGui_SetNextWindowPos((game->screen_width - 440) * 0.5f, 120.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(440.0f, 280.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.34f);
  if (!ShroomImGui_Begin("Leaderboard", NULL,
                         SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                             SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_Text("Mushroom Cap Leaderboard");
  ShroomImGui_Text("Tab or Enter closes the leaderboard.");
  ShroomImGui_Separator();

  for (size_t index = 0; index < shown_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[leaderboard[index].index];
    const PlayerThreatState threat_state = GetThreatState(&game->world, game->local_player, player);
    const char* label = GetPlayerDisplayName(game, player);
    const Color color =
        player == game->local_player ? RAYWHITE : GetThreatOutlineColor(threat_state);

    ShroomImGui_TextColored(ToImGuiColor(color), TextFormat("%d. %s   %.0f mass", (int)(index + 1),
                                                            label, player->mass));
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
  {
    const uint32_t display_rtt = game->net.rtt_ms > 9999u ? 9999u : game->net.rtt_ms;
    const uint32_t display_avg =
        game->net.rtt_average_ms > 9999u ? 9999u : game->net.rtt_average_ms;
    ShroomImGui_TextColored(ToImGuiColor(GetLatencyColor(display_avg)),
                            TextFormat("Ping: %ums  Avg: %ums", display_rtt, display_avg));
  }
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

  ShroomImGui_SetNextWindowPos((game->screen_width - 400.0f) * 0.5f,
                               (game->screen_height - 200.0f) * 0.5f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(400.0f, 200.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.92f);
  if (!ShroomImGui_Begin("Connection Status", NULL,
                         SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                             SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS |
                             SHROOM_IMGUI_WINDOW_NO_SCROLLBAR)) {
    ShroomImGui_End();
    return;
  }

  const char* status_text = "Connecting...";
  Color status_color = (Color){200, 200, 200, 255};

  if (game->net.status == CLIENT_NET_ERROR) {
    status_text = "Connection Error";
    status_color = (Color){255, 100, 100, 255};
  } else if (game->net.status == CLIENT_NET_DISCONNECTED) {
    status_text = "Disconnected";
    status_color = (Color){255, 150, 100, 255};
  } else if (game->net.status == CLIENT_NET_CONNECTED && !game->net.handshake_received) {
    status_text = "Handshaking...";
    status_color = (Color){100, 200, 255, 255};
  } else if (game->net.status == CLIENT_NET_CONNECTING) {
    status_text = "Connecting...";
    status_color = (Color){200, 200, 200, 255};
  }

  ShroomImGui_TextColored(ToImGuiColor(status_color), status_text);

  if (game->net.status_text[0] != '\0') {
    ShroomImGui_TextWrapped(game->net.status_text);
  }

  ShroomImGui_Text(TextFormat("Target: %s:%u", game->selected_server_host,
                              (unsigned int)game->selected_server_port));

  ShroomImGui_Spacing();

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
  if (!game->diagnostics_overlay_open || (game->local_player == NULL)) {
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
  {
    const uint32_t display_rtt = game->net.rtt_ms > 9999u ? 9999u : game->net.rtt_ms;
    const uint32_t display_avg =
        game->net.rtt_average_ms > 9999u ? 9999u : game->net.rtt_average_ms;
    ShroomImGui_Text(TextFormat("RTT: %ums  Avg: %ums", display_rtt, display_avg));
  }

  ShroomImGui_End();
}

static const float kChatInactiveTimeout = 8.0f;

typedef struct ClientProfileStats {
  ShroomProfileWindow frame;
  ShroomProfileWindow update;
  ShroomProfileWindow draw;
  ShroomProfileWindow network;
  ShroomProfileWindow snapshot_prediction;
  double pending_update_ms;
  uint64_t last_log_ms;
} ClientProfileStats;

static ClientProfileStats g_client_profile;

static void DrawFungalHudPanel(Rectangle rect, Color accent);

static uint64_t ClientProfileNowNanos(void) { return (uint64_t)(GetTime() * 1000000000.0); }

static void ClientProfileMaybeLog(void) {
  const uint64_t now_ms = ClientProfileNowNanos() / 1000000ull;

  if (!ShroomProfileEnabled()) {
    return;
  }
  if ((g_client_profile.last_log_ms != 0ull) &&
      ((now_ms - g_client_profile.last_log_ms) < 5000ull)) {
    return;
  }

  g_client_profile.last_log_ms = now_ms;
  printf("profile,client,frame_avg_ms=%.3f,frame_peak_ms=%.3f,update_avg_ms=%.3f,"
         "update_peak_ms=%.3f,draw_avg_ms=%.3f,draw_peak_ms=%.3f,net_avg_ms=%.3f,"
         "net_peak_ms=%.3f,snapshot_prediction_avg_ms=%.3f,snapshot_prediction_peak_ms=%.3f\n",
         ShroomProfileAverageMs(&g_client_profile.frame), g_client_profile.frame.peak_ms,
         ShroomProfileAverageMs(&g_client_profile.update), g_client_profile.update.peak_ms,
         ShroomProfileAverageMs(&g_client_profile.draw), g_client_profile.draw.peak_ms,
         ShroomProfileAverageMs(&g_client_profile.network), g_client_profile.network.peak_ms,
         ShroomProfileAverageMs(&g_client_profile.snapshot_prediction),
         g_client_profile.snapshot_prediction.peak_ms);
  ShroomProfileResetPeak(&g_client_profile.frame);
  ShroomProfileResetPeak(&g_client_profile.update);
  ShroomProfileResetPeak(&g_client_profile.draw);
  ShroomProfileResetPeak(&g_client_profile.network);
  ShroomProfileResetPeak(&g_client_profile.snapshot_prediction);
}

static void UpdateChatState(Game* game, float delta_time) {
  if (!IsOnlineMode(game->active_mode) || game->chat_minimized) {
    return;
  }
  if (game->chat_open) {
    game->chat_inactive_timer = 0.0f;
  } else {
    game->chat_inactive_timer += delta_time;
    if (game->chat_inactive_timer >= kChatInactiveTimeout) {
      game->chat_minimized = true;
      game->chat_inactive_timer = 0.0f;
    }
  }
}

static void DrawChatDock(Game* game) {
  const float dock_width = 392.0f;
  const float history_height = game->chat_open ? 178.0f : 160.0f;
  const float input_height = game->chat_open ? 98.0f : 0.0f;
  const float total_height = history_height + input_height + (game->chat_open ? 8.0f : 48.0f);
  const float pos_x = (float)game->screen_width - dock_width - 18.0f;
  const float pos_y = 132.0f;
  const float btn_size = 44.0f;
  const float btn_panel_size = btn_size + 8.0f;
  const Color hud_accent = (Color){130, 210, 150, 255};
  const Color unread_accent = (Color){214, 178, 92, 255};

  if (!IsOnlineMode(game->active_mode)) {
    return;
  }

  /* Minimized state: frame the chat button with the same fungal HUD panel
   * treatment as gameplay panels so it reads as part of the HUD. */
  if (game->chat_minimized) {
    const bool has_unread = game->net.chat_unread_count > 0u;
    const Color accent = has_unread ? unread_accent : hud_accent;
    const float panel_x = (float)game->screen_width - btn_panel_size - 18.0f;
    const char* label =
        has_unread
            ? TextFormat("%u", game->net.chat_unread_count > 9u ? 9u : game->net.chat_unread_count)
            : "C";
    DrawFungalHudPanel((Rectangle){panel_x, pos_y, btn_panel_size, btn_panel_size}, accent);
    ShroomImGui_PushStyleColor(SHROOM_IMGUI_COL_BUTTON, 0.09f, 0.12f, 0.09f, 0.72f);
    ShroomImGui_PushStyleColor(SHROOM_IMGUI_COL_BUTTON_HOVERED, 0.21f, 0.34f, 0.19f, 0.88f);
    ShroomImGui_PushStyleColor(SHROOM_IMGUI_COL_BUTTON_ACTIVE, 0.36f, 0.55f, 0.31f, 0.96f);
    ShroomImGui_PushWindowPadding(0.0f, 0.0f);
    ShroomImGui_PushWindowRounding(8.0f);
    ShroomImGui_SetNextWindowPos(panel_x + 4.0f, pos_y + 4.0f, SHROOM_IMGUI_COND_ALWAYS);
    ShroomImGui_SetNextWindowSize(btn_size, btn_size, SHROOM_IMGUI_COND_ALWAYS);
    ShroomImGui_SetNextWindowBgAlpha(0.0f);
    if (ShroomImGui_Begin("ChatBtn", NULL,
                          SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                              SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                              SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS |
                              SHROOM_IMGUI_WINDOW_NO_SCROLLBAR)) {
      if (ShroomImGui_Button(label, btn_size, btn_size)) {
        game->chat_minimized = false;
        game->chat_inactive_timer = 0.0f;
      }
    }
    ShroomImGui_End();
    ShroomImGui_PopStyleVar();   /* rounding */
    ShroomImGui_PopStyleVar();   /* padding */
    ShroomImGui_PopStyleColor(); /* active */
    ShroomImGui_PopStyleColor(); /* hovered */
    ShroomImGui_PopStyleColor(); /* button */
    return;
  }

  DrawFungalHudPanel((Rectangle){pos_x, pos_y, dock_width, total_height}, hud_accent);
  ShroomImGui_PushWindowPadding(12.0f, 10.0f);
  ShroomImGui_PushWindowRounding(8.0f);
  ShroomImGui_SetNextWindowPos(pos_x, pos_y, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(dock_width, total_height, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.30f);
  if (!ShroomImGui_Begin("Chat", NULL,
                         SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                             SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    ShroomImGui_PopStyleVar(); /* rounding */
    ShroomImGui_PopStyleVar(); /* padding */
    return;
  }

  /* History scroll region. */
  ShroomImGui_BeginChild("ChatHistory", 0.0f, history_height, false);
  {
    const uint32_t count = game->net.chat_history_count;
    if (count > 0u) {
      const uint32_t start_index =
          (count < SHROOM_CLIENT_CHAT_HISTORY_COUNT)
              ? 0u
              : game->net.chat_history_head % SHROOM_CLIENT_CHAT_HISTORY_COUNT;
      uint32_t i;
      for (i = 0u; i < count; ++i) {
        const ChatMessage* msg =
            &game->net.chat_history[(start_index + i) % SHROOM_CLIENT_CHAT_HISTORY_COUNT];
        const bool is_local =
            (game->local_player != NULL) && (msg->sender_id == game->net.player_id);
        if (msg->timestamp_sec > 0u) {
          time_t ts = (time_t)msg->timestamp_sec;
          struct tm tm_local = {0};
#if defined(_WIN32)
          localtime_s(&tm_local, &ts);
#else
          localtime_r(&ts, &tm_local);
#endif
          ShroomImGui_TextColored(ToImGuiColor((Color){130, 130, 130, 200}),
                                  TextFormat("%02d:%02d", tm_local.tm_hour, tm_local.tm_min));
          ShroomImGui_SameLine();
        }
        ShroomImGui_TextColored(
            ToImGuiColor(is_local ? (Color){180, 230, 180, 255} : (Color){210, 210, 255, 255}),
            TextFormat("%.31s", msg->sender_name));
        ShroomImGui_SameLine();
        ShroomImGui_TextWrapped(msg->message);
      }
    }
  }
  if (game->chat_scroll_to_bottom) {
    ShroomImGui_SetScrollHereY(1.0f);
    game->chat_scroll_to_bottom = false;
  }
  ShroomImGui_EndChild();

  if (game->chat_open) {
    bool send;
    ShroomImGui_Spacing();
    if (game->chat_focus_input) {
      ShroomImGui_SetKeyboardFocusHere();
      game->chat_focus_input = false;
    }
    send = ShroomImGui_InputTextWithSubmit("##chatinput", game->chat_input_buf,
                                           sizeof(game->chat_input_buf), "Send");
    if (send && game->chat_input_buf[0] != '\0') {
      if (ClientNetSendChat(&game->net, game->net.player_id,
                            game->local_player ? game->local_player->name : "",
                            game->chat_input_buf)) {
        game->chat_input_buf[0] = '\0';
        game->chat_scroll_to_bottom = true;
        game->chat_focus_input = true;
      }
    }
    ShroomImGui_Text("Enter sends   Esc closes");
  } else {
    if (game->net.chat_unread_count > 0u) {
      ShroomImGui_TextColored(ToImGuiColor(unread_accent), TextFormat("%u unread  Enter/T to type",
                                                                      game->net.chat_unread_count));
    } else {
      ShroomImGui_Text("Enter/T to type");
    }
  }

  ShroomImGui_End();
  ShroomImGui_PopStyleVar(); /* rounding */
  ShroomImGui_PopStyleVar(); /* padding */
}

static void DrawFungalHudPanel(Rectangle rect, Color accent) {
  DrawRectangleRounded((Rectangle){rect.x + 3.0f, rect.y + 4.0f, rect.width, rect.height}, 0.18f, 8,
                       Fade(BLACK, 0.32f));
  DrawRectangleRounded(rect, 0.18f, 8, Fade((Color){24, 31, 22, 255}, 0.72f));
  DrawRectangleRoundedLines(rect, 0.18f, 8, Fade(accent, 0.70f));
  DrawCircleV((Vector2){rect.x + 18.0f, rect.y + 9.0f}, 7.0f, Fade(accent, 0.34f));
  DrawCircleV((Vector2){rect.x + rect.width - 20.0f, rect.y + rect.height - 10.0f}, 5.0f,
              Fade((Color){214, 178, 92, 255}, 0.28f));
  DrawLineEx((Vector2){rect.x + 12.0f, rect.y + rect.height - 9.0f},
             (Vector2){rect.x + rect.width - 14.0f, rect.y + rect.height - 9.0f}, 1.5f,
             Fade((Color){94, 154, 94, 255}, 0.28f));
}

static void DrawGameplayHud(const Game* game, int local_rank, size_t leaderboard_count,
                            ShroomZone zone) {
  if (game->local_player == NULL) {
    return;
  }

  const ClientHudDensity density = game->settings.hud_density;

  if (density == CLIENT_HUD_MINIMAL) {
    DrawFungalHudPanel((Rectangle){18.0f, 18.0f, 180.0f, 60.0f}, GetZoneColor(zone));
    ShroomImGui_SetNextWindowPos(18.0f, 18.0f, SHROOM_IMGUI_COND_ALWAYS);
    ShroomImGui_SetNextWindowSize(180.0f, 60.0f, SHROOM_IMGUI_COND_ALWAYS);
    ShroomImGui_SetNextWindowBgAlpha(0.30f);
    if (ShroomImGui_Begin("HUD Minimal", NULL,
                          SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                              SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                              SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS |
                              SHROOM_IMGUI_WINDOW_NO_SCROLLBAR)) {
      ShroomImGui_Text(TextFormat("Spore Mass %.0f", game->local_player->mass));
      ShroomImGui_TextColored(ToImGuiColor(GetZoneColor(zone)), GetZoneLabel(zone));
    }
    ShroomImGui_End();
    return;
  }

  if (density == CLIENT_HUD_COMPACT) {
    DrawFungalHudPanel((Rectangle){18.0f, 18.0f, 220.0f, 110.0f}, GetZoneColor(zone));
    ShroomImGui_SetNextWindowPos(18.0f, 18.0f, SHROOM_IMGUI_COND_ALWAYS);
    ShroomImGui_SetNextWindowSize(220.0f, 110.0f, SHROOM_IMGUI_COND_ALWAYS);
    ShroomImGui_SetNextWindowBgAlpha(0.30f);
    if (ShroomImGui_Begin("HUD Compact", NULL,
                          SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                              SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                              SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS |
                              SHROOM_IMGUI_WINDOW_NO_SCROLLBAR)) {
      ShroomImGui_Text(TextFormat("Cap Mass %.0f", game->local_player->mass));
      ShroomImGui_Text(TextFormat("Rank %d/%d",
                                  local_rank > 0 ? local_rank : (int)leaderboard_count,
                                  (int)leaderboard_count));
      ShroomImGui_Spacing();
      ShroomImGui_TextColored(ToImGuiColor(GetZoneColor(zone)), GetZoneLabel(zone));
      if (game->local_piece_count > 1) {
        ShroomImGui_TextColored(ToImGuiColor(YELLOW),
                                TextFormat("Pieces %d", game->local_piece_count));
      }
    }
    ShroomImGui_End();
    return;
  }

  DrawFungalHudPanel((Rectangle){18.0f, 18.0f, 280.0f, 168.0f}, GetZoneColor(zone));
  ShroomImGui_SetNextWindowPos(18.0f, 18.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(280.0f, 168.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.30f);
  if (ShroomImGui_Begin("HUD Left", NULL,
                        SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                            SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                            SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS |
                            SHROOM_IMGUI_WINDOW_NO_SCROLLBAR)) {
    ShroomImGui_Text(TextFormat("Mycelium Mass %.0f", game->local_player->mass));
    ShroomImGui_Text(TextFormat("Canopy Rank %d/%d",
                                local_rank > 0 ? local_rank : (int)leaderboard_count,
                                (int)leaderboard_count));
    ShroomImGui_TextColored(ToImGuiColor(GetZoneColor(zone)),
                            TextFormat("Zone %s", GetZoneLabel(zone)));
    ShroomImGui_Spacing();
    if (IsDecayMassActive(&game->world, game->local_player)) {
      const float decay_threshold = zone == SHROOM_ZONE_CENTER ? (SHROOM_DEFAULT_PLAYER_MASS * 2.0f)
                                    : zone == SHROOM_ZONE_MID  ? SHROOM_DECAY_MASS_THRESHOLD
                                                               : SHROOM_MAX_PLAYER_MASS;
      ShroomImGui_TextColored(
          ToImGuiColor(RED),
          TextFormat("Decaying  excess %.0f", game->local_player->mass - decay_threshold));
    }
    if (game->local_piece_count > 1) {
      ShroomImGui_TextColored(ToImGuiColor(YELLOW),
                              TextFormat("Pieces %d", game->local_piece_count));
    }
    if ((game->local_player->speed_powerup_timer > 0.0f) ||
        (game->local_player->shield_powerup_timer > 0.0f)) {
      ShroomImGui_Spacing();
    }
    if (game->local_player->speed_powerup_timer > 0.0f) {
      ShroomImGui_TextColored(ToImGuiColor(SKYBLUE), "Speed Burst");
    }
    if (game->local_player->shield_powerup_timer > 0.0f) {
      ShroomImGui_TextColored(ToImGuiColor(VIOLET), "Mass Shield");
    }
  }
  ShroomImGui_End();

  DrawFungalHudPanel((Rectangle){game->screen_width - 180.0f, 18.0f, 162.0f, 98.0f},
                     (Color){130, 210, 150, 255});
  ShroomImGui_SetNextWindowPos(game->screen_width - 180.0f, 18.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(162.0f, 98.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.30f);
  if (ShroomImGui_Begin("HUD Right", NULL,
                        SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                            SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                            SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS |
                            SHROOM_IMGUI_WINDOW_NO_SCROLLBAR)) {
    const int fps = GetFPS();
    Color fps_color = GREEN;
    if (fps < 30) {
      fps_color = RED;
    } else if (fps < 60) {
      fps_color = YELLOW;
    }
    ShroomImGui_TextColored(ToImGuiColor(fps_color), TextFormat("FPS %d", fps));
    if (game->settings.show_ping_ms && IsOnlineMode(game->active_mode)) {
      const uint32_t display_rtt =
          game->net.rtt_average_ms > 9999u ? 9999u : game->net.rtt_average_ms;
      ShroomImGui_TextColored(ToImGuiColor(GetLatencyColor(display_rtt)),
                              TextFormat("Ping %ums", display_rtt));
    }
    ShroomImGui_Spacing();
    ShroomImGui_Text(TextFormat("Players %d", (int)game->world.player_count));
  }
  ShroomImGui_End();
}

static void DrawProximityMap(const Game* game) {
  if (game->local_player == NULL) {
    return;
  }
  const Vector2 center = {98.0f, game->screen_height - 172.0f};
  const float inner_radius = kProximityMapRadius - 10.0f;
  const float pulse_phase = 0.5f + (0.5f * sinf(game->inspect_prompt_timer * 3.6f));
  const float pulse = 0.68f + (0.32f * pulse_phase);
  const float sweep_radius = inner_radius * (0.70f + 0.14f * pulse);
  const ShroomVec2 local_position = game->local_player->position;

  DrawFungalHudPanel(
      (Rectangle){center.x - kProximityMapRadius - 12.0f, center.y - kProximityMapRadius - 14.0f,
                  (kProximityMapRadius + 12.0f) * 2.0f, (kProximityMapRadius + 54.0f) * 2.0f},
      (Color){112, 196, 120, 255});
  DrawCircleV(center, kProximityMapRadius, Fade((Color){28, 42, 34, 255}, 0.88f));
  DrawCircleV(center, kProximityMapRadius - 4.0f, Fade((Color){52, 86, 62, 255}, 0.18f));
  DrawCircleLinesV(center, kProximityMapRadius, Fade((Color){168, 222, 126, 255}, 0.62f));
  DrawCircleLinesV(center, kProximityMapRadius * 0.66f, Fade((Color){112, 182, 126, 255}, 0.45f));
  DrawCircleLinesV(center, kProximityMapRadius * 0.33f, Fade((Color){140, 208, 156, 255}, 0.32f));
  DrawCircleLinesV(center, sweep_radius, Fade((Color){180, 230, 140, 255}, 0.18f + pulse * 0.22f));
  DrawLineV((Vector2){center.x - inner_radius, center.y},
            (Vector2){center.x + inner_radius, center.y}, Fade(RAYWHITE, 0.14f));
  DrawLineV((Vector2){center.x, center.y - inner_radius},
            (Vector2){center.x, center.y + inner_radius}, Fade(RAYWHITE, 0.14f));

  for (size_t index = 0; index < game->world.spore_count; ++index) {
    const ShroomSporeState* spore = &game->world.spores[index];
    ShroomVec2 delta;
    float distance_sqr;
    Vector2 map_position;
    float scale;

    if (!ShroomClientShouldSampleIndexedItem(index, game->world.spore_count,
                                             SHROOM_CLIENT_PROXIMITY_SPORE_DOT_BUDGET)) {
      continue;
    }
    delta = ShroomVec2Sub(spore->position, local_position);
    distance_sqr = ShroomVec2LengthSqr(delta);

    if (!spore->active || (distance_sqr > (kProximityMapRange * kProximityMapRange))) {
      continue;
    }

    scale = inner_radius / kProximityMapRange;
    map_position = (Vector2){center.x + (delta.x * scale), center.y + (delta.y * scale)};
    if (Vector2DistanceSqr(center, map_position) > (inner_radius * inner_radius)) {
      continue;
    }

    DrawCircleV(map_position, 3.0f, Fade((Color){255, 225, 138, 255}, 0.94f));
  }

  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];
    const PlayerThreatState threat_state = GetThreatState(&game->world, game->local_player, player);
    const ShroomVec2 delta = ShroomVec2Sub(player->position, local_position);
    const float distance_sqr = ShroomVec2LengthSqr(delta);
    Vector2 map_position;
    float scale;
    float dot_radius;
    Color dot_color;

    if (!player->alive || (player == game->local_player) ||
        (distance_sqr > (kProximityMapRange * kProximityMapRange))) {
      continue;
    }

    scale = inner_radius / kProximityMapRange;
    map_position = (Vector2){center.x + (delta.x * scale), center.y + (delta.y * scale)};
    if (Vector2DistanceSqr(center, map_position) > (inner_radius * inner_radius)) {
      continue;
    }

    dot_radius = player->is_bot ? 3.8f : 4.4f;
    dot_color = player->is_bot ? Fade(GetPlayerFillColor(game, player), 0.96f)
                               : Fade(GetThreatOutlineColor(threat_state), 0.97f);
    if (threat_state == PLAYER_THREAT_DANGER) {
      dot_radius += pulse * 1.6f;
    }

    DrawCircleV(map_position, dot_radius, dot_color);
    if (threat_state == PLAYER_THREAT_PREY) {
      DrawCircleLinesV(map_position, dot_radius + 2.0f, Fade(SKYBLUE, 0.45f));
    }
  }

  DrawCircleV(center, 5.5f, RAYWHITE);
  DrawCircleLinesV(center, 8.0f, Fade((Color){150, 228, 255, 255}, 0.88f));
  {
    const char* header = "MYCELIUM";
    const int header_size = 14;
    const int header_width = MeasureText(header, header_size);
    const float header_y = center.y + kProximityMapRadius + 14.0f;
    const char* threat_label = "Threat";
    const char* prey_label = "Prey";
    const int row_size = 12;
    const float row_y = center.y + kProximityMapRadius + 40.0f;
    const float swatch_radius = 4.0f;
    const float col_gap = 8.0f;
    const int threat_w = MeasureText(threat_label, row_size);
    const int prey_w = MeasureText(prey_label, row_size);
    const float threat_entry_w = swatch_radius * 2.0f + col_gap + (float)threat_w;
    const float prey_entry_w = swatch_radius * 2.0f + col_gap + (float)prey_w;
    const float pair_w = threat_entry_w + 16.0f + prey_entry_w;
    const float threat_x = center.x - pair_w * 0.5f;
    const float prey_x = center.x - pair_w * 0.5f + threat_entry_w + 16.0f;

    DrawText(header, (int)(center.x - (float)header_width * 0.5f), (int)header_y, header_size,
             Fade((Color){226, 245, 188, 255}, 0.94f));
    DrawCircleV((Vector2){threat_x + swatch_radius, row_y + (float)row_size * 0.5f}, swatch_radius,
                Fade(RED, 0.94f));
    DrawText(threat_label, (int)(threat_x + swatch_radius * 2.0f + col_gap), (int)row_y, row_size,
             Fade((Color){255, 236, 236, 255}, 0.90f));
    DrawCircleV((Vector2){prey_x + swatch_radius, row_y + (float)row_size * 0.5f}, swatch_radius,
                Fade(SKYBLUE, 0.96f));
    DrawText(prey_label, (int)(prey_x + swatch_radius * 2.0f + col_gap), (int)row_y, row_size,
             Fade((Color){232, 245, 255, 255}, 0.90f));
  }

  // Match timer display (online mode only)
  if (IsOnlineMode(game->active_mode) && game->net.welcome_received) {
    const float time_remaining = game->net.match_time_remaining;
    const int minutes = (int)(time_remaining / 60.0f);
    const int seconds = (int)time_remaining % 60;

    // Determine timer color based on time remaining
    Color timer_color = (Color){255, 255, 255, 255}; // White
    Color timer_bg = (Color){40, 40, 40, 200};       // Dark background
    if (time_remaining < 10.0f) {
      timer_color = (Color){255, 100, 100, 255}; // Red
      timer_bg = (Color){60, 20, 20, 220};       // Dark red background
    } else if (time_remaining < 60.0f) {
      timer_color = (Color){255, 200, 100, 255}; // Yellow/Orange
      timer_bg = (Color){60, 50, 20, 220};       // Dark yellow background
    }

    // Format timer text
    char timer_text[16];
    snprintf(timer_text, sizeof(timer_text), "%d:%02d", minutes, seconds);

    const int font_size = 28;
    const int text_width = MeasureText(timer_text, font_size);
    const float padding = 20.0f;
    const float timer_width = (float)text_width + padding * 2.0f;
    const float timer_height = (float)font_size + padding * 2.0f;
    const float timer_x = (game->screen_width - timer_width) * 0.5f;
    const float timer_y = 20.0f;

    // Draw background panel with rounded corners
    DrawRectangleRounded((Rectangle){timer_x, timer_y, timer_width, timer_height}, 0.3f, 8,
                         timer_bg);
    DrawRectangleRoundedLines((Rectangle){timer_x, timer_y, timer_width, timer_height}, 0.3f, 8,
                              Fade(timer_color, 0.6f));

    // Draw timer text centered
    const float text_x = timer_x + (timer_width - (float)text_width) * 0.5f;
    const float text_y = timer_y + (timer_height - (float)font_size) * 0.5f;
    DrawText(timer_text, (int)text_x, (int)text_y, font_size, timer_color);
  }
}

static void DrawSpectatorOverlay(Game* game, const LeaderboardEntry* leaderboard,
                                 size_t leaderboard_count) {
  const ShroomPlayerState* target;
  const ShroomLobbyEntry* current_lobby;
  int target_rank = 0;
  int current_lobby_index = -1;
  float panel_width;
  float panel_height;
  float panel_x;
  float panel_y;

  if ((game == NULL) || !game->spectator_mode) {
    return;
  }

  panel_width = game->screen_width < 760 ? (float)game->screen_width - 32.0f : 330.0f;
  if (panel_width < 260.0f) {
    panel_width = 260.0f;
  }
  panel_height = game->screen_height < 560 ? 206.0f : 238.0f;
  panel_x = (float)game->screen_width - panel_width - 18.0f;
  if (panel_x < 16.0f) {
    panel_x = 16.0f;
  }
  panel_y = (float)game->screen_height - panel_height - 18.0f;
  if (panel_y < 96.0f) {
    panel_y = 96.0f;
  }

  target = FindSpectatorTarget(game);
  if (target != NULL) {
    target_rank = GetPlayerRank(game, target);
  }
  current_lobby = FindCurrentLobbyEntry(game, &current_lobby_index);

  ShroomImGui_SetNextWindowPos(panel_x, panel_y, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(panel_width, panel_height, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.64f);
  if (!ShroomImGui_Begin("Spectator", NULL,
                         SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                             SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  if ((current_lobby != NULL) || (game->net.lobby_name[0] != '\0')) {
    ShroomImGui_Text(
        TextFormat("Lobby %s", current_lobby != NULL ? current_lobby->name : game->net.lobby_name));
  } else {
    ShroomImGui_Text("Local Spectator");
  }
  ShroomImGui_Text(game->spectator_follow_mode ? "Follow Camera" : "Free Camera");
  ShroomImGui_TextWrapped(
      "Tab cycles players. F toggles follow/free. WASD/arrows pan free camera.");
  if (IsOnlineMode(game->active_mode) && (game->net.lobby_count > 1u)) {
    if (ShroomImGui_Button("Prev Lobby", 96.0f, 24.0f)) {
      GameSwitchSpectatorLobby(game, -1);
    }
    ShroomImGui_SameLine();
    if (ShroomImGui_Button("Next Lobby", 96.0f, 24.0f)) {
      GameSwitchSpectatorLobby(game, 1);
    }
    ShroomImGui_SameLine();
    ShroomImGui_Text(TextFormat("%d/%u", current_lobby_index >= 0 ? current_lobby_index + 1 : 0,
                                game->net.lobby_count));
  }
  ShroomImGui_Separator();
  if (target != NULL) {
    ShroomImGui_TextColored(ToImGuiColor((Color){190, 235, 150, 255}),
                            TextFormat("Watching > %s", GetPlayerDisplayName(game, target)));
    ShroomImGui_Text(
        TextFormat("Mass %.0f  Rank %d  Zone %s", target->mass,
                   target_rank > 0 ? target_rank : (int)leaderboard_count,
                   GetZoneLabel(ShroomGetZoneAtPosition(&game->world, target->position))));
  } else {
    ShroomImGui_TextWrapped("No living colonies to follow. Switch to free camera or return later.");
  }
  ShroomImGui_Separator();

  ShroomImGui_BeginChild("SpectatorTargets", 0.0f, 82.0f, false);
  for (size_t row = 0; row < leaderboard_count && row < 6u; ++row) {
    const ShroomPlayerState* player = &game->world.players[leaderboard[row].index];
    char label[96];
    const bool selected = player->entity_id == game->spectated_entity_id;
    snprintf(label, sizeof(label), "%s%zu. %s  %.0f##spectate%u", selected ? "> " : "  ", row + 1,
             GetPlayerDisplayName(game, player), player->mass, player->entity_id);
    if (selected) {
      ShroomImGui_PushStyleColor(SHROOM_IMGUI_COL_BUTTON, 0.24f, 0.36f, 0.16f, 0.86f);
      ShroomImGui_PushStyleColor(SHROOM_IMGUI_COL_BUTTON_HOVERED, 0.34f, 0.50f, 0.23f, 0.94f);
      ShroomImGui_PushStyleColor(SHROOM_IMGUI_COL_BUTTON_ACTIVE, 0.42f, 0.62f, 0.28f, 1.0f);
    }
    if (ShroomImGui_Button(label, panel_width - 34.0f, 22.0f)) {
      game->spectated_entity_id = player->entity_id;
      game->spectator_follow_mode = true;
    }
    if (selected) {
      ShroomImGui_PopStyleColor();
      ShroomImGui_PopStyleColor();
      ShroomImGui_PopStyleColor();
    }
  }
  ShroomImGui_EndChild();

  ShroomImGui_End();
}

static const ShroomPlayerState* GetInputReferencePlayer(const Game* game) {
  const ShroomPlayerState* input_ref = game->local_player;

  if (game->focused_piece_entity_id != 0) {
    size_t pi;
    for (pi = 0; pi < game->world.player_count; ++pi) {
      const ShroomPlayerState* p = &game->world.players[pi];
      if (p->alive && (p->entity_id == game->focused_piece_entity_id)) {
        return p;
      }
    }
  }

  return input_ref;
}

static ShroomVec2 GetMovementInput(const Game* game) {
  const Vector2 mouse_screen = GetMousePosition();
  const Vector2 mouse_world = GetScreenToWorld2D(mouse_screen, game->camera);
  /* Use the focused piece as the movement origin so that after Tab-switching
   * to a split fragment the mouse direction is correct for that piece. */
  const ShroomPlayerState* input_ref = GetInputReferencePlayer(game);
  Vector2 player_world = {0.0f, 0.0f};
  Vector2 movement;

  if (input_ref != NULL) {
    player_world = (Vector2){input_ref->position.x, input_ref->position.y};
  }

  movement = Vector2Subtract(mouse_world, player_world);

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

static ShroomVec2 GetSplitAimInput(const Game* game, ShroomVec2 fallback_direction) {
  const Vector2 mouse_screen = GetMousePosition();
  const Vector2 mouse_world = GetScreenToWorld2D(mouse_screen, game->camera);
  const ShroomPlayerState* input_ref = GetInputReferencePlayer(game);
  Vector2 player_world = {0.0f, 0.0f};
  Vector2 aim;

  if (input_ref != NULL) {
    player_world = (Vector2){input_ref->position.x, input_ref->position.y};
  }

  aim = Vector2Subtract(mouse_world, player_world);
  if (game->settings.invert_mouse) {
    aim = Vector2Scale(aim, -1.0f);
  }

  if (Vector2LengthSqr(aim) > 16.0f) {
    aim = Vector2Normalize(aim);
    return (ShroomVec2){aim.x, aim.y};
  }

  if (ShroomVec2LengthSqr(fallback_direction) > 0.0001f) {
    const float length = sqrtf(ShroomVec2LengthSqr(fallback_direction));
    return (ShroomVec2){fallback_direction.x / length, fallback_direction.y / length};
  }
  return (ShroomVec2){1.0f, 0.0f};
}

static bool LocalCanConsumeMouseTarget(const Game* game) {
  const ShroomPlayerState* local = game != NULL ? GetInputReferencePlayer(game) : NULL;
  const Vector2 mouse_world =
      game != NULL ? GetScreenToWorld2D(GetMousePosition(), game->camera) : (Vector2){0};
  const ShroomVec2 mouse_pos = {mouse_world.x, mouse_world.y};

  if ((game == NULL) || (local == NULL) || !local->alive) {
    return false;
  }

  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* target = &game->world.players[index];

    if (!target->alive || (target->player_id == local->player_id)) {
      continue;
    }
    if (ShroomDistanceSqr(mouse_pos, target->position) > (target->radius * target->radius)) {
      continue;
    }
    if (ShroomPlayerCanConsume(&game->world, local, target)) {
      return true;
    }
  }

  return false;
}

static ShroomCursorMode GetGameCursorMode(const Game* game) {
  const ShroomPlayerState* local = game != NULL ? GetInputReferencePlayer(game) : NULL;

  if ((game == NULL) || game->spectator_mode || IsOverlayBlockingGameplay(game) ||
      game->chat_open || (local == NULL) || !local->alive) {
    return SHROOM_CURSOR_DISABLED;
  }
  if (LocalCanConsumeMouseTarget(game)) {
    return SHROOM_CURSOR_CONSUME;
  }
  if ((game->split_hold_timer > 0.0f) || game->split_requested) {
    return SHROOM_CURSOR_SPLIT;
  }

  return SHROOM_CURSOR_GAMEPLAY;
}

static uint32_t FindHoveredLocalPieceEntity(const Game* game) {
  const ShroomPlayerId local_pid =
      game != NULL && game->local_player != NULL ? game->local_player->player_id : 0u;
  const Vector2 mouse_world =
      game != NULL ? GetScreenToWorld2D(GetMousePosition(), game->camera) : (Vector2){0};
  const ShroomVec2 mouse_pos = {mouse_world.x, mouse_world.y};

  if (local_pid == 0u) {
    return 0u;
  }

  for (size_t index = 0; index < game->world.player_count; ++index) {
    const ShroomPlayerState* player = &game->world.players[index];

    if (!player->alive || (player->player_id != local_pid)) {
      continue;
    }
    if (ShroomDistanceSqr(mouse_pos, player->position) <= (player->radius * player->radius)) {
      return player->entity_id;
    }
  }

  return 0u;
}

static void UpdateSpectatorCamera(Game* game, float delta_time) {
  const ShroomPlayerState* target;
  Vector2 camera_target;
  Vector2 pan = {0.0f, 0.0f};
  float pan_speed;

  if ((game == NULL) || !game->spectator_mode) {
    return;
  }

  pan_speed = 640.0f / fmaxf(game->camera.zoom, 0.35f);

  target = FindSpectatorTarget(game);
  if (game->spectator_follow_mode && (target != NULL)) {
    camera_target = (Vector2){target->position.x, target->position.y};
    game->camera.target =
        Vector2Lerp(game->camera.target, camera_target, Clamp(delta_time * 7.0f, 0.0f, 1.0f));
    return;
  }

  game->spectator_follow_mode = false;
  if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
    pan.x += 1.0f;
  }
  if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
    pan.x -= 1.0f;
  }
  if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
    pan.y += 1.0f;
  }
  if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
    pan.y -= 1.0f;
  }
  if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    pan =
        Vector2Subtract(pan, Vector2Scale(GetMouseDelta(), 1.0f / fmaxf(game->camera.zoom, 0.35f)));
  }
  if (Vector2LengthSqr(pan) > 1.0f) {
    pan = Vector2Normalize(pan);
  }
  game->camera.target = Vector2Add(game->camera.target, Vector2Scale(pan, pan_speed * delta_time));
  game->camera.target.x = Clamp(game->camera.target.x, 0.0f, game->world.width);
  game->camera.target.y = Clamp(game->camera.target.y, 0.0f, game->world.height);
}

void GameInit(Game* game, int screen_width, int screen_height, GameSessionMode mode) {
  size_t bot_index;
  ClientSettings settings = game->settings;
  GameSessionMode selected_mode = game->selected_mode;
  bool start_in_spectator_mode = game->start_in_spectator_mode;
  char selected_server_host[sizeof(game->selected_server_host)] = {0};
  uint16_t selected_server_port = game->selected_server_port;

  snprintf(selected_server_host, sizeof(selected_server_host), "%s",
           game->selected_server_host[0] != '\0' ? game->selected_server_host : "127.0.0.1");
  if (selected_server_port == 0) {
    selected_server_port = SHROOM_SERVER_PORT;
  }

  if (mode == SHROOM_SESSION_MODE_LOBBY_PLAY) {
    /* Connection and auth already done via lobby browser.
     * Preserve the entire net state and just set up rendering. */
    ClientNetState saved_net = game->net;
    *game = (Game){0};
    game->net = saved_net;
    game->settings = settings;
    game->selected_mode = selected_mode;
    game->start_in_spectator_mode = start_in_spectator_mode;
    game->active_mode = mode;
    game->screen_width = screen_width;
    game->screen_height = screen_height;
    game->camera.offset = (Vector2){screen_width / 2.0f, screen_height / 2.0f};
    game->camera.target = (Vector2){game->net.world_width * 0.5f, game->net.world_height * 0.5f};
    game->camera.zoom = game->settings.camera_zoom;
    game->camera_zoom_target = game->settings.camera_zoom;
    game->diagnostics_overlay_open = game->settings.diagnostics_enabled;
    game->session_start_time = (float)GetTime();
    game->session_duration_seconds = 0u;
    game->peak_mass = 0.0f;
    game->final_mass = 0.0f;
    game->final_rank = 0;
    game->show_results = false;
    ShroomWorldInit(&game->world);
    CaptureParticleBaselines(game);
    ShroomClientAudioEnsureAllSfxLoaded();
    ShroomCursorHideSystem();
    /* local_player is NULL until first snapshot from server. */
    return;
  }

  *game = (Game){0};
  game->settings = settings;
  game->selected_mode = selected_mode;
  game->start_in_spectator_mode = start_in_spectator_mode;
  game->active_mode = mode;
  snprintf(game->selected_server_host, sizeof(game->selected_server_host), "%s",
           selected_server_host);
  game->selected_server_port = selected_server_port;
  game->screen_width = screen_width;
  game->screen_height = screen_height;
  game->session_start_time = (float)GetTime();
  game->session_duration_seconds = 0u;

  if (mode == SHROOM_SESSION_MODE_QUICK_PLAY) {
    ClientNetInit(&game->net, game->selected_server_host, game->selected_server_port);
  } else {
    game->net.status = CLIENT_NET_CONNECTED;
    snprintf(game->net.status_text, sizeof(game->net.status_text), "%s", "Offline");
  }
  ShroomWorldInit(&game->world);
  game->local_player = ShroomWorldSpawnPlayer(&game->world, 1, false);
  if (game->local_player != NULL) {
    snprintf(game->local_player->name, sizeof(game->local_player->name), "%s",
             (mode == SHROOM_SESSION_MODE_QUICK_PLAY) ? "local-client" : "You");
  }
  for (bot_index = 0; bot_index < SHROOM_BOT_COUNT; ++bot_index) {
    ShroomPlayerState* bot =
        ShroomWorldSpawnPlayer(&game->world, (ShroomPlayerId)(bot_index + 2), true);
    if (bot != NULL) {
      snprintf(bot->name, sizeof(bot->name), "Bot %zu", bot_index + 1);
    }
  }

  game->camera.offset = (Vector2){screen_width / 2.0f, screen_height / 2.0f};
  game->camera.target = game->local_player != NULL ? (Vector2){game->local_player->position.x,
                                                               game->local_player->position.y}
                                                   : (Vector2){0};
  game->camera.rotation = 0.0f;
  game->camera.zoom = game->settings.camera_zoom;
  game->camera_zoom_target = game->settings.camera_zoom;
  if (game->local_player != NULL) {
    game->current_zone = ShroomGetZoneAtPosition(&game->world, game->local_player->position);
    game->previous_local_position = game->local_player->position;
    game->previous_local_mass = game->local_player->mass;
  }
  game->zone_callout_timer = kStatusBannerDuration;
  game->diagnostics_overlay_open = game->settings.diagnostics_enabled;
  CaptureParticleBaselines(game);
  if (game->start_in_spectator_mode) {
    game->start_in_spectator_mode = false;
    GameEnterSpectatorMode(game);
  }
  ShroomClientAudioEnsureAllSfxLoaded();
  ShroomCursorHideSystem();
}

void GameHandleResize(Game* game, int screen_width, int screen_height) {
  if (game == NULL) {
    return;
  }

  if (screen_width < 640) {
    screen_width = 640;
  }
  if (screen_height < 360) {
    screen_height = 360;
  }

  if ((game->screen_width == screen_width) && (game->screen_height == screen_height)) {
    return;
  }

  game->screen_width = screen_width;
  game->screen_height = screen_height;
  game->camera.offset = (Vector2){(float)screen_width * 0.5f, (float)screen_height * 0.5f};
}

void GameUpdate(Game* game, float delta_time) {
  ShroomVec2 input_direction;
  ShroomVec2 split_aim_direction;
  const uint32_t previous_input_sequence = game->net.last_input_sequence;
  const bool profile_enabled = ShroomProfileEnabled();
  const uint64_t update_start_nanos = profile_enabled ? ClientProfileNowNanos() : 0ull;

  GameHandleResize(game, GetScreenWidth(), GetScreenHeight());
  ShroomClientAudioUpdateMusic(&game->settings);

  if (!game->spectator_mode && !IsOverlayBlockingGameplay(game) && !game->chat_open) {
    const uint32_t hovered_piece = FindHoveredLocalPieceEntity(game);
    if (hovered_piece != 0u) {
      game->focused_piece_entity_id = hovered_piece;
      game->piece_focus_changed = true;
    }
  }

  if (game->spectator_mode || IsOverlayBlockingGameplay(game) || game->chat_open) {
    input_direction = (ShroomVec2){0};
  } else {
    input_direction = GetMovementInput(game);
  }
  split_aim_direction = GetSplitAimInput(game, input_direction);
  game->split_aim_direction = split_aim_direction;
  if (ShroomVec2LengthSqr(game->split_aim_visual_direction) <= 0.0001f) {
    game->split_aim_visual_direction = split_aim_direction;
  } else {
    const float blend = Clamp(delta_time * 26.0f, 0.0f, 1.0f);
    Vector2 visual = {game->split_aim_visual_direction.x, game->split_aim_visual_direction.y};
    const Vector2 target = {split_aim_direction.x, split_aim_direction.y};
    visual = Vector2Lerp(visual, target, blend);
    if (Vector2LengthSqr(visual) > 0.0001f) {
      visual = Vector2Normalize(visual);
      game->split_aim_visual_direction = (ShroomVec2){visual.x, visual.y};
    } else {
      game->split_aim_visual_direction = split_aim_direction;
    }
  }

  /* Scroll-wheel zoom — guarded so inspect overlay and chat scrolling are unaffected. */
  if (!IsKeyDown(KEY_I) && !game->chat_open && !IsOverlayBlockingGameplay(game)) {
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
      game->camera_zoom_target = Clamp(game->camera_zoom_target + wheel * 0.1f, 0.35f, 2.0f);
      game->settings.camera_zoom = game->camera_zoom_target;
      ClientSettingsSave(&game->settings);
    }
  }
  game->camera.zoom += (game->camera_zoom_target - game->camera.zoom) * delta_time * 9.0f;

  /* Update local piece count and validate focused piece. */
  {
    ShroomPlayerId local_pid = game->local_player != NULL ? game->local_player->player_id : 0;
    bool focused_alive = (game->focused_piece_entity_id == 0);
    size_t pi;

    game->local_piece_count = 0;
    for (pi = 0; pi < game->world.player_count; ++pi) {
      const ShroomPlayerState* p = &game->world.players[pi];
      if (p->alive && (p->player_id == local_pid)) {
        game->local_piece_count++;
        if (p->entity_id == game->focused_piece_entity_id) {
          focused_alive = true;
        }
      }
    }
    if (!focused_alive) {
      game->focused_piece_entity_id = 0;
      game->piece_focus_changed = false;
    }
    /* If only one piece remains, clear focus so Tab goes back to leaderboard. */
    if (game->local_piece_count <= 1) {
      game->focused_piece_entity_id = 0;
      game->piece_focus_changed = false;
    }
    /* Set local_has_split when we first detect two pieces. */
    if (game->local_piece_count > 1) {
      game->local_has_split = true;
    }
    /* Reset on respawn: single piece at default mass means a fresh life. */
    if ((game->local_piece_count == 1) && (game->local_player != NULL) &&
        (game->local_player->mass <= SHROOM_DEFAULT_PLAYER_MASS * 1.5f)) {
      game->local_has_split = false;
    }
  }

  if (IsOnlineMode(game->active_mode)) {
    if (!game->spectator_mode && game->split_requested && (game->local_player != NULL) &&
        game->local_player->alive && (game->local_player->mass >= SHROOM_SPLIT_MIN_MASS) &&
        !game->local_player->has_split) {
      QueueGameplaySfx(game, SHROOM_CLIENT_SFX_SPLIT, 0.64f);
    }
    const uint64_t network_start_nanos = profile_enabled ? ClientProfileNowNanos() : 0ull;
    ClientNetUpdate(&game->net, input_direction, !game->spectator_mode && game->split_requested,
                    !game->spectator_mode && game->eject_requested, split_aim_direction,
                    game->focused_piece_entity_id, delta_time);
    if (profile_enabled) {
      ShroomProfileRecord(&g_client_profile.network,
                          ShroomProfileNanosToMs(ClientProfileNowNanos() - network_start_nanos));
    }
    game->split_requested = false;
    game->eject_requested = false;
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
    const uint64_t snapshot_start_nanos = profile_enabled ? ClientProfileNowNanos() : 0ull;
    ApplyNetworkSnapshot(game);
    /* Apply prediction to the focused piece, not necessarily local_player. */
    {
      ShroomPlayerState* predicted = game->local_player;
      if (game->focused_piece_entity_id != 0) {
        size_t pi;
        for (pi = 0; pi < game->world.player_count; ++pi) {
          ShroomPlayerState* p = &game->world.players[pi];
          if (p->alive && (p->entity_id == game->focused_piece_entity_id)) {
            predicted = p;
            break;
          }
        }
      }
      if (!game->spectator_mode && (predicted != NULL)) {
        ApplyPredictedInputToPlayer(&game->world, predicted, input_direction, delta_time);
      }
    }
    if (profile_enabled) {
      ShroomProfileRecord(&g_client_profile.snapshot_prediction,
                          ShroomProfileNanosToMs(ClientProfileNowNanos() - snapshot_start_nanos));
    }
  } else {
    /* Offline: find the focused piece to apply player input to, mark others as AI. */
    {
      ShroomPlayerState* ctrl = game->local_player;
      ShroomPlayerId local_pid = game->local_player != NULL ? game->local_player->player_id : 0;
      size_t pi;

      if (game->focused_piece_entity_id != 0 && (local_pid != 0)) {
        for (pi = 0; pi < game->world.player_count; ++pi) {
          ShroomPlayerState* p = &game->world.players[pi];
          if (p->alive && (p->entity_id == game->focused_piece_entity_id)) {
            ctrl = p;
            break;
          }
        }
      }

      /* Human split pieces without focus keep their last direction; bot pieces keep AI. */
      if (local_pid != 0) {
        for (pi = 0; pi < game->world.player_count; ++pi) {
          ShroomPlayerState* p = &game->world.players[pi];
          if (p->alive && (p->player_id == local_pid)) {
            p->ai_controlled = p->is_bot && (ctrl != NULL) && (p != ctrl);
          }
        }
      }

      if (game->spectator_mode) {
        if (ctrl != NULL) {
          ctrl->ai_controlled = true;
        }
      } else {
        ShroomPlayerSetInput(ctrl, input_direction);
      }
      if (!game->spectator_mode && game->split_requested && (ctrl != NULL)) {
        if (ShroomWorldSplitPlayerToward(&game->world, ctrl, split_aim_direction)) {
          QueueGameplaySfx(game, SHROOM_CLIENT_SFX_SPLIT, 0.64f);
        }
      }
      if (!game->spectator_mode && game->eject_requested && (ctrl != NULL)) {
        ShroomWorldEjectMass(&game->world, ctrl, split_aim_direction);
      }
      game->split_requested = false;
      game->eject_requested = false;
      ShroomWorldStep(&game->world, delta_time);
    }
  }

  EmitGameplayEventParticles(game);
  DispatchQueuedGameplayEvents(game);
  UpdateAmbientParticles(game, delta_time);
  UpdateGameplayParticles(game, delta_time);
  SyncRenderPositions(game, delta_time);
  UpdateStatusBanners(game, delta_time);
  UpdateCombatFeedback(game);
  UpdateCombatNotifications(game, delta_time);
  UpdateDeathCutscene(game, delta_time);
  UpdateInspectOverlay(game, delta_time);
  UpdateChatState(game, delta_time);
  if (game->spectator_mode) {
    UpdateSpectatorCamera(game, delta_time);
    game->piece_focus_changed = false;
  } else if (game->death_camera_hold_timer > 0.0f) {
    game->death_camera_hold_timer = fmaxf(0.0f, game->death_camera_hold_timer - delta_time);
    game->camera.target = game->death_camera_hold_pos;
    game->piece_focus_changed = false;
  } else if (game->local_player != NULL) {
    const ShroomPlayerState* cam_target = game->local_player;
    if (game->focused_piece_entity_id != 0) {
      size_t pi;
      for (pi = 0; pi < game->world.player_count; ++pi) {
        const ShroomPlayerState* p = &game->world.players[pi];
        if (p->alive && (p->entity_id == game->focused_piece_entity_id)) {
          cam_target = p;
          break;
        }
      }
    }
    game->camera.target = (Vector2){cam_target->position.x, cam_target->position.y};
    game->piece_focus_changed = false;
  }

  /* Track peak mass for results screen */
  if (game->local_player != NULL && game->local_player->mass > game->peak_mass) {
    game->peak_mass = game->local_player->mass;
  }

  if (profile_enabled) {
    g_client_profile.pending_update_ms =
        ShroomProfileNanosToMs(ClientProfileNowNanos() - update_start_nanos);
    ShroomProfileRecord(&g_client_profile.update, g_client_profile.pending_update_ms);
  }
}

void GameDraw(Game* game) {
  LeaderboardEntry leaderboard[SHROOM_MAX_PLAYERS];
  size_t leaderboard_count = 0;
  size_t shown_count;
  const Rectangle view_bounds = GetCameraWorldBounds(game->camera);
  const ShroomZone zone = (game->local_player != NULL)
                              ? ShroomGetZoneAtPosition(&game->world, game->local_player->position)
                              : SHROOM_ZONE_OUTER;
  int local_rank;
  const bool profile_enabled = ShroomProfileEnabled();
  const uint64_t draw_start_nanos = profile_enabled ? ClientProfileNowNanos() : 0ull;

  BuildLeaderboard(game, leaderboard, &leaderboard_count);
  local_rank = GetLocalPlayerRank(game, leaderboard, leaderboard_count);
  shown_count = leaderboard_count < 8 ? leaderboard_count : 8;

  ClearBackground((Color){18, 18, 26, 255});

  BeginMode2D(game->camera);
  DrawArenaZones(&game->world, view_bounds);
  DrawBoundaryConsumeWarning(game, view_bounds);
  DrawRectangleLines(0, 0, (int)game->world.width, (int)game->world.height, Fade(DARKGREEN, 0.7f));
  DrawGrid(80, 64.0f);
  DrawSpores(&game->world, view_bounds);
  DrawPowerups(&game->world, view_bounds);
  DrawGameplayParticles(game, view_bounds);
  DrawPlayers(game, view_bounds);
  EndMode2D();

  DrawOffscreenIndicators(game);
  DrawProximityMap(game);
  DrawGameplayHud(game, local_rank, leaderboard_count, zone);
  DrawSpectatorOverlay(game, leaderboard, shown_count);
  DrawInspectPrompt(game);
  DrawStatusBanners(game);
  DrawCombatNotifications(game);
  DrawDeathCutscene(game);
  DrawLeaderboardOverlay(game, leaderboard, shown_count);
  DrawMenuOverlay(game);
  DrawLeaveConfirmationOverlay(game);
  DrawConnectionOverlay(game);
  DrawDiagnosticsOverlay(game);
  DrawChatDock(game);
  DrawInspectOverlay(game);
  DrawKillFeed(game);
  ShroomCursorDraw(GetGameCursorMode(game));

  if (profile_enabled) {
    const double draw_ms = ShroomProfileNanosToMs(ClientProfileNowNanos() - draw_start_nanos);
    ShroomProfileRecord(&g_client_profile.draw, draw_ms);
    ShroomProfileRecord(&g_client_profile.frame, g_client_profile.pending_update_ms + draw_ms);
    ClientProfileMaybeLog();
  }
}

void GameShutdown(Game* game) {
  if (IsOnlineMode(game->active_mode)) {
    ClientNetShutdown(&game->net);
  }
  ShroomClientAudioShutdown();
  ShroomCursorShowSystem();
}
