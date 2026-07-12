#include "sim.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static ShroomRoundStats* ShroomWorldEnsureRoundStats(ShroomWorldState* world,
                                                     ShroomPlayerId player_id);

static float ShroomClamp(float value, float min_value, float max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }

  return value;
}

static float ShroomLerp(float start, float end, float t) { return start + ((end - start) * t); }

static uint64_t ShroomWorldCurrentTimeMs(const ShroomWorldState* world) {
  if (world == NULL) {
    return 0u;
  }
  return (uint64_t)(((double)world->tick * 1000.0) / (double)SHROOM_SERVER_TICK_RATE);
}

static uint64_t ShroomWorldStepEndTimeMs(const ShroomWorldState* world, float delta_time) {
  const double delta_ms = (double)delta_time * 1000.0;
  return ShroomWorldCurrentTimeMs(world) + (uint64_t)(delta_ms > 0.0 ? delta_ms : 0.0);
}

static uint32_t ShroomNormalizeSeed(uint32_t seed) { return seed == 0 ? 0x6d2b79f5u : seed; }

static uint32_t ShroomNextRandom(ShroomWorldState* world) {
  uint32_t x = world->random_state;

  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  world->random_state = x;
  return x;
}

static float ShroomRandomFloat(ShroomWorldState* world, float min_value, float max_value) {
  const float normalized = (float)ShroomNextRandom(world) / (float)UINT32_MAX;
  return min_value + (normalized * (max_value - min_value));
}

float ShroomDistanceSqr(ShroomVec2 a, ShroomVec2 b) {
  const ShroomVec2 delta = ShroomVec2Sub(a, b);
  return ShroomVec2LengthSqr(delta);
}

static ShroomVec2 ShroomNormalizeOrZero(ShroomVec2 v) {
  const float length_sqr = ShroomVec2LengthSqr(v);
  float scale;

  if (length_sqr <= 0.0001f) {
    return (ShroomVec2){0};
  }

  scale = 1.0f / __builtin_sqrtf(length_sqr);
  return ShroomVec2Scale(v, scale);
}

static ShroomVec2 ShroomWorldCenter(const ShroomWorldState* world) {
  return (ShroomVec2){world->width * 0.5f, world->height * 0.5f};
}

static size_t ShroomSpawnPopulation(const ShroomWorldState* world) {
  size_t count = 0;

  for (size_t index = 0; index < world->player_count; ++index) {
    const ShroomPlayerState* player = &world->players[index];

    if (player->alive && (player->piece_index == 0)) {
      count += 1;
    }
  }

  return count;
}

static float ShroomSpawnDensity(const ShroomWorldState* world) {
  const size_t population = ShroomSpawnPopulation(world);

  if (population <= 1u) {
    return 0.0f;
  }

  return ShroomClamp((float)(population - 1u) / (float)(SHROOM_MAX_PARTICIPANTS - 1u), 0.0f, 1.0f);
}

static float ShroomSpawnSafeDistance(const ShroomWorldState* world) {
  const size_t population = ShroomSpawnPopulation(world);
  float t;

  if (population <= SHROOM_SPAWN_SMALL_LOBBY_PLAYERS) {
    return SHROOM_SPAWN_SAFE_DISTANCE;
  }

  if (population <= SHROOM_SPAWN_MEDIUM_LOBBY_PLAYERS) {
    t = (float)(population - SHROOM_SPAWN_SMALL_LOBBY_PLAYERS) /
        (float)(SHROOM_SPAWN_MEDIUM_LOBBY_PLAYERS - SHROOM_SPAWN_SMALL_LOBBY_PLAYERS);
    return ShroomLerp(SHROOM_SPAWN_SAFE_DISTANCE, SHROOM_SPAWN_MEDIUM_SAFE_DISTANCE, t);
  }

  t = (float)(population - SHROOM_SPAWN_MEDIUM_LOBBY_PLAYERS) /
      (float)(SHROOM_MAX_PARTICIPANTS - SHROOM_SPAWN_MEDIUM_LOBBY_PLAYERS);
  return ShroomLerp(SHROOM_SPAWN_MEDIUM_SAFE_DISTANCE, SHROOM_SPAWN_MIN_SAFE_DISTANCE,
                    ShroomClamp(t, 0.0f, 1.0f));
}

static float ShroomSpawnOuterRadius(const ShroomWorldState* world) {
  const float max_radius = (fminf(world->width, world->height) * 0.5f) - SHROOM_SPAWN_PADDING;
  const float min_radius = fminf(SHROOM_SPAWN_MIN_RADIUS, max_radius);

  return ShroomLerp(max_radius, min_radius, ShroomSpawnDensity(world));
}

static ShroomVec2 ShroomRandomSpawnCandidate(ShroomWorldState* world, bool prefer_outer) {
  const ShroomVec2 center = ShroomWorldCenter(world);
  const float min_x = SHROOM_SPAWN_PADDING;
  const float min_y = SHROOM_SPAWN_PADDING;
  const float max_x = world->width - SHROOM_SPAWN_PADDING;
  const float max_y = world->height - SHROOM_SPAWN_PADDING;

  if (prefer_outer) {
    const float density = ShroomSpawnDensity(world);
    const float outer_radius = ShroomSpawnOuterRadius(world);
    const float band_width =
        ShroomLerp(SHROOM_SPAWN_MAX_BAND_WIDTH, SHROOM_SPAWN_MIN_BAND_WIDTH, density);
    const float inner_radius = fmaxf(SHROOM_ZONE_MID_RADIUS, outer_radius - band_width);
    const float distance =
        sqrtf(ShroomRandomFloat(world, inner_radius * inner_radius, outer_radius * outer_radius));
    const float angle = ShroomRandomFloat(world, 0.0f, 6.2831853f);

    return (ShroomVec2){ShroomClamp(center.x + (cosf(angle) * distance), min_x, max_x),
                        ShroomClamp(center.y + (sinf(angle) * distance), min_y, max_y)};
  }

  return (ShroomVec2){ShroomRandomFloat(world, min_x, max_x),
                      ShroomRandomFloat(world, min_y, max_y)};
}

static ShroomVec2 ShroomRandomPointInZone(ShroomWorldState* world, ShroomZone zone) {
  const ShroomVec2 center = ShroomWorldCenter(world);
  const float min_x = 60.0f;
  const float min_y = 60.0f;
  const float max_x = world->width - 60.0f;
  const float max_y = world->height - 60.0f;
  const float inner_radius = zone == SHROOM_ZONE_CENTER ? 0.0f
                             : zone == SHROOM_ZONE_MID  ? SHROOM_ZONE_CENTER_RADIUS
                                                        : SHROOM_ZONE_MID_RADIUS;
  const float outer_radius = zone == SHROOM_ZONE_CENTER ? SHROOM_ZONE_CENTER_RADIUS
                             : zone == SHROOM_ZONE_MID  ? SHROOM_ZONE_MID_RADIUS
                                                        : fminf(world->width, world->height) * 0.5f;
  const float distance =
      sqrtf(ShroomRandomFloat(world, inner_radius * inner_radius, outer_radius * outer_radius));
  const float angle = ShroomRandomFloat(world, 0.0f, 6.2831853f);

  return (ShroomVec2){ShroomClamp(center.x + (cosf(angle) * distance), min_x, max_x),
                      ShroomClamp(center.y + (sinf(angle) * distance), min_y, max_y)};
}

ShroomZone ShroomGetZoneAtPosition(const ShroomWorldState* world, ShroomVec2 position) {
  const float distance_sqr = ShroomDistanceSqr(position, ShroomWorldCenter(world));

  if (distance_sqr <= (SHROOM_ZONE_CENTER_RADIUS * SHROOM_ZONE_CENTER_RADIUS)) {
    return SHROOM_ZONE_CENTER;
  }
  if (distance_sqr <= (SHROOM_ZONE_MID_RADIUS * SHROOM_ZONE_MID_RADIUS)) {
    return SHROOM_ZONE_MID;
  }

  return SHROOM_ZONE_OUTER;
}

float ShroomGetConsumeMassAdvantageAtPosition(const ShroomWorldState* world, ShroomVec2 position) {
  if (ShroomGetZoneAtPosition(world, position) == SHROOM_ZONE_CENTER) {
    return SHROOM_CENTER_CONSUME_ADVANTAGE;
  }

  return SHROOM_CONSUME_MASS_ADVANTAGE;
}

static bool ShroomIsSafeSpawn(const ShroomWorldState* world, ShroomVec2 position) {
  size_t index;
  const float safe_distance = ShroomSpawnSafeDistance(world);

  for (index = 0; index < world->player_count; ++index) {
    const ShroomPlayerState* other = &world->players[index];
    const float safety_radius = other->radius + safe_distance;

    if (!other->alive) {
      continue;
    }

    if (ShroomDistanceSqr(position, other->position) < (safety_radius * safety_radius)) {
      return false;
    }
  }

  return true;
}

static ShroomVec2 ShroomRandomSpawnPosition(ShroomWorldState* world, bool prefer_outer) {
  size_t attempt;
  ShroomVec2 best_candidate = ShroomRandomSpawnCandidate(world, prefer_outer);
  float best_clearance_sqr = -1.0f;

  for (attempt = 0; attempt < 64; ++attempt) {
    const ShroomVec2 candidate = ShroomRandomSpawnCandidate(world, prefer_outer);
    const ShroomZone zone = ShroomGetZoneAtPosition(world, candidate);
    float nearest_sqr = INFINITY;

    for (size_t index = 0; index < world->player_count; ++index) {
      const ShroomPlayerState* other = &world->players[index];

      if (!other->alive) {
        continue;
      }
      nearest_sqr = fminf(nearest_sqr, ShroomDistanceSqr(candidate, other->position));
    }

    if (prefer_outer && zone != SHROOM_ZONE_OUTER) {
      continue;
    }
    if (nearest_sqr > best_clearance_sqr) {
      best_candidate = candidate;
      best_clearance_sqr = nearest_sqr;
    }
    if (ShroomIsSafeSpawn(world, candidate)) {
      return candidate;
    }
  }

  return best_candidate;
}

static void ShroomRespawnPlayer(ShroomWorldState* world, ShroomPlayerState* player) {
  player->position = ShroomRandomSpawnPosition(world, true);
  player->input_direction = (ShroomVec2){0};
  player->mass = SHROOM_DEFAULT_PLAYER_MASS;
  player->radius = ShroomMassToRadius(player->mass);
  player->decay_spore_accumulator = 0.0f;
  player->last_move_time_ms = ShroomWorldCurrentTimeMs(world);
  player->alive = true;
  player->ai_controlled = false;
  player->has_split = false;
  player->split_velocity = (ShroomVec2){0};
  player->merge_timer = 0.0f;
  player->spawn_protection_timer = player->is_bot ? 0.0f : SHROOM_PLAYER_SPAWN_PROTECTION_SECONDS;
  player->speed_powerup_timer = 0.0f;
  player->shield_powerup_timer = 0.0f;
  player->magnet_powerup_timer = 0.0f;
  player->decay_immune_powerup_timer = 0.0f;
  player->eject_cooldown_timer = 0.0f;
  player->piece_index = 0;
}

static void ShroomSpawnOrResetSpore(ShroomWorldState* world, ShroomSporeState* spore) {
  const float zone_roll = ShroomRandomFloat(world, 0.0f, 1.0f);
  const ShroomZone zone = zone_roll < 0.30f   ? SHROOM_ZONE_OUTER
                          : zone_roll < 0.65f ? SHROOM_ZONE_MID
                                              : SHROOM_ZONE_CENTER;
  const ShroomVec2 candidate = ShroomRandomPointInZone(world, zone);

  spore->position = candidate;
  spore->value = SHROOM_SPORE_VALUE;
  spore->active = true;
  if (spore->entity_id == 0) {
    spore->entity_id = world->next_entity_id++;
  }
}

static void ShroomSpawnDecaySpore(ShroomWorldState* world, ShroomVec2 position, uint16_t value) {
  ShroomSporeState* spore;

  if ((world == NULL) || (value == 0u)) {
    return;
  }

  if (world->spore_count < SHROOM_MAX_SPORES) {
    spore = &world->spores[world->spore_count++];
  } else {
    spore = &world->spores[ShroomNextRandom(world) % world->spore_count];
  }

  *spore = (ShroomSporeState){
      .entity_id = spore->entity_id != 0 ? spore->entity_id : world->next_entity_id++,
      .position = position,
      .value = value,
      .active = true,
  };
}

static void ShroomInitializeSpores(ShroomWorldState* world) {
  size_t index;

  world->spore_count = SHROOM_SPORE_TARGET_COUNT;
  for (index = 0; index < world->spore_count; ++index) {
    ShroomSpawnOrResetSpore(world, &world->spores[index]);
  }
}

static void ShroomSpawnOrResetPowerup(ShroomWorldState* world, ShroomPowerupState* powerup,
                                      size_t slot_index) {
  const ShroomZone zone = (slot_index % 4u) == 0u   ? SHROOM_ZONE_OUTER
                          : (slot_index % 4u) == 1u ? SHROOM_ZONE_MID
                                                    : SHROOM_ZONE_CENTER;

  powerup->position = ShroomRandomPointInZone(world, zone);
  powerup->type = (ShroomPowerupType)(1u + (ShroomNextRandom(world) % SHROOM_POWERUP_TYPE_COUNT));
  powerup->respawn_timer = 0.0f;
  powerup->active = true;
  if (powerup->entity_id == 0) {
    powerup->entity_id = world->next_entity_id++;
  }
}

static void ShroomInitializePowerups(ShroomWorldState* world) {
  world->powerup_count = SHROOM_MAX_POWERUPS;
  for (size_t index = 0; index < world->powerup_count; ++index) {
    ShroomSpawnOrResetPowerup(world, &world->powerups[index], index);
  }
}

static float ShroomBotSporeZoneWeight(float bot_mass, ShroomZone zone) {
  if (bot_mass < (SHROOM_DEFAULT_PLAYER_MASS * 1.25f)) {
    switch (zone) {
    case SHROOM_ZONE_OUTER:
      return 1.15f;
    case SHROOM_ZONE_MID:
      return 1.0f;
    case SHROOM_ZONE_CENTER:
      return 0.65f;
    }
  }

  if (bot_mass > (SHROOM_DEFAULT_PLAYER_MASS * 1.8f)) {
    switch (zone) {
    case SHROOM_ZONE_OUTER:
      return 0.8f;
    case SHROOM_ZONE_MID:
      return 1.0f;
    case SHROOM_ZONE_CENTER:
      return 1.35f;
    }
  }

  switch (zone) {
  case SHROOM_ZONE_OUTER:
    return 0.95f;
  case SHROOM_ZONE_MID:
    return 1.05f;
  case SHROOM_ZONE_CENTER:
    return 1.0f;
  }

  return 1.0f;
}

static float ShroomBotCenterPressureWeight(float bot_mass, ShroomZone zone) {
  if (bot_mass <= (SHROOM_DEFAULT_PLAYER_MASS * 1.5f)) {
    return zone == SHROOM_ZONE_OUTER ? 0.25f : 0.45f;
  }

  return zone == SHROOM_ZONE_OUTER ? 1.2f : 0.8f;
}

ShroomBotRiskProfile ShroomBotProfileForPlayer(ShroomPlayerId player_id) {
  if (player_id == 0u) {
    return SHROOM_BOT_PROFILE_CONSERVATIVE;
  }
  return (ShroomBotRiskProfile)((player_id - 1u) % (uint32_t)SHROOM_BOT_PROFILE_COUNT);
}

static float ShroomBotPowerupUtility(const ShroomPlayerState* bot,
                                     const ShroomPowerupState* powerup,
                                     ShroomBotRiskProfile profile) {
  float utility = 1.0f;

  switch (powerup->type) {
  case SHROOM_POWERUP_SPEED:
    utility = profile == SHROOM_BOT_PROFILE_AGGRESSIVE ? 1.45f : 1.1f;
    if (bot->speed_powerup_timer > 1.0f) {
      utility *= 0.25f;
    }
    break;
  case SHROOM_POWERUP_SHIELD:
    utility = profile == SHROOM_BOT_PROFILE_CONSERVATIVE ? 1.7f : 1.2f;
    if (bot->shield_powerup_timer > 1.0f) {
      utility *= 0.25f;
    }
    break;
  case SHROOM_POWERUP_MAGNET:
    utility = profile == SHROOM_BOT_PROFILE_OBJECTIVE ? 1.65f : 1.05f;
    if (bot->magnet_powerup_timer > 1.0f) {
      utility *= 0.25f;
    }
    break;
  case SHROOM_POWERUP_DECAY_IMMUNE:
    utility = bot->mass >= SHROOM_DECAY_MASS_THRESHOLD ? 1.4f : 0.8f;
    if (bot->decay_immune_powerup_timer > 1.0f) {
      utility *= 0.25f;
    }
    break;
  default:
    return 0.0f;
  }

  if (profile == SHROOM_BOT_PROFILE_OBJECTIVE) {
    utility *= 1.5f;
  }
  return utility;
}

static float ShroomBotTacticalCooldown(ShroomBotRiskProfile profile) {
  switch (profile) {
  case SHROOM_BOT_PROFILE_AGGRESSIVE:
    return SHROOM_BOT_TACTICAL_COOLDOWN_AGGRESSIVE;
  case SHROOM_BOT_PROFILE_OBJECTIVE:
    return SHROOM_BOT_TACTICAL_COOLDOWN_OBJECTIVE;
  case SHROOM_BOT_PROFILE_CONSERVATIVE:
  case SHROOM_BOT_PROFILE_COUNT:
    return SHROOM_BOT_TACTICAL_COOLDOWN_CONSERVATIVE;
  }
  return SHROOM_BOT_TACTICAL_COOLDOWN_CONSERVATIVE;
}

static bool ShroomBotTryTacticalAction(ShroomWorldState* world, ShroomPlayerState* bot,
                                       const ShroomPlayerState* prey, float prey_distance_sqr,
                                       const ShroomPlayerState* threat, float threat_distance_sqr,
                                       const ShroomPowerupState* objective) {
  const ShroomBotRiskProfile profile = ShroomBotProfileForPlayer(bot->player_id);
  const bool threat_close = (threat != NULL) && (threat_distance_sqr < (700.0f * 700.0f));
  ShroomVec2 direction;

  if ((bot->piece_index != 0u) || (bot->bot_tactical_cooldown_timer > 0.0f)) {
    return false;
  }

  if ((prey != NULL) && !ShroomPlayerHasConsumeProtection(prey) && !threat_close &&
      (prey_distance_sqr <= (SHROOM_BOT_SPLIT_REACH * SHROOM_BOT_SPLIT_REACH)) &&
      ShroomPlayerCanSplit(world, bot)) {
    const float post_split_mass = bot->mass * (1.0f - SHROOM_SPLIT_MASS_LOSS_FRACTION) * 0.5f;
    float safety_margin = 1.2f;
    bool profile_allows_split = true;

    if (profile == SHROOM_BOT_PROFILE_CONSERVATIVE) {
      safety_margin = 1.45f;
      profile_allows_split = bot->shield_powerup_timer > 0.0f;
    } else if (profile == SHROOM_BOT_PROFILE_AGGRESSIVE) {
      safety_margin = 1.03f;
    } else {
      safety_margin = 1.18f;
    }

    if (profile_allows_split &&
        (post_split_mass >= prey->mass *
                                ShroomGetConsumeMassAdvantageAtPosition(world, prey->position) *
                                safety_margin)) {
      direction = ShroomVec2Sub(prey->position, bot->position);
      if (ShroomWorldSplitPlayerToward(world, bot, direction)) {
        bot->bot_tactical_cooldown_timer = ShroomBotTacticalCooldown(profile);
        return true;
      }
    }
  }

  if (profile == SHROOM_BOT_PROFILE_AGGRESSIVE && (prey != NULL) &&
      !ShroomPlayerHasConsumeProtection(prey) && !threat_close &&
      (prey_distance_sqr <= (SHROOM_BOT_EJECT_PRESSURE_RANGE * SHROOM_BOT_EJECT_PRESSURE_RANGE))) {
    const float post_eject_mass =
        bot->mass - SHROOM_EJECT_MASS_VALUE * (1.0f + SHROOM_EJECT_COST_FRACTION);
    if (post_eject_mass >=
        prey->mass * ShroomGetConsumeMassAdvantageAtPosition(world, prey->position) * 1.1f) {
      direction = ShroomVec2Sub(prey->position, bot->position);
      if (ShroomWorldEjectMass(world, bot, direction)) {
        bot->bot_tactical_cooldown_timer = ShroomBotTacticalCooldown(profile);
        return true;
      }
    }
  }

  if ((profile == SHROOM_BOT_PROFILE_OBJECTIVE) && (objective != NULL) && !threat_close &&
      (bot->mass >= SHROOM_EJECT_MIN_MASS * 1.15f)) {
    direction = ShroomVec2Sub(bot->position, objective->position);
    if (ShroomWorldEjectMass(world, bot, direction)) {
      bot->bot_tactical_cooldown_timer = ShroomBotTacticalCooldown(profile);
      return true;
    }
  }

  return false;
}

#define SHROOM_SPORE_GRID_CELL_SIZE 200.0f
#define SHROOM_SPORE_GRID_COLS (((int)(SHROOM_WORLD_WIDTH / SHROOM_SPORE_GRID_CELL_SIZE)) + 1)
#define SHROOM_SPORE_GRID_ROWS (((int)(SHROOM_WORLD_HEIGHT / SHROOM_SPORE_GRID_CELL_SIZE)) + 1)
#define SHROOM_SPORE_GRID_CELLS (SHROOM_SPORE_GRID_COLS * SHROOM_SPORE_GRID_ROWS)
#define SHROOM_SPORE_GRID_MAX_PER_CELL 64

/* Bots only meaningfully pursue spores inside this radius; the score formula decays as
 * 1/distance^2 and the "give up" threshold corresponds to roughly 220 units, so 600 units
 * covers the useful range while keeping the per-bot cell scan bounded. */
#define SHROOM_BOT_SPORE_SEARCH_RADIUS 600.0f

typedef struct ShroomSporeGridCell {
  uint16_t indices[SHROOM_SPORE_GRID_MAX_PER_CELL];
  uint16_t count;
} ShroomSporeGridCell;

static void ShroomBuildSporeGrid(const ShroomWorldState* world, ShroomSporeGridCell* grid) {
  size_t spore_index;

  for (size_t i = 0; i < SHROOM_SPORE_GRID_CELLS; ++i) {
    grid[i].count = 0;
  }

  for (spore_index = 0; spore_index < world->spore_count; ++spore_index) {
    const ShroomSporeState* spore = &world->spores[spore_index];
    int col, row, cell;

    if (!spore->active) {
      continue;
    }

    col = (int)(spore->position.x / SHROOM_SPORE_GRID_CELL_SIZE);
    row = (int)(spore->position.y / SHROOM_SPORE_GRID_CELL_SIZE);

    if (col < 0)
      col = 0;
    if (col >= SHROOM_SPORE_GRID_COLS)
      col = SHROOM_SPORE_GRID_COLS - 1;
    if (row < 0)
      row = 0;
    if (row >= SHROOM_SPORE_GRID_ROWS)
      row = SHROOM_SPORE_GRID_ROWS - 1;

    cell = row * SHROOM_SPORE_GRID_COLS + col;
    if (grid[cell].count < SHROOM_SPORE_GRID_MAX_PER_CELL) {
      grid[cell].indices[grid[cell].count++] = (uint16_t)spore_index;
    }
  }
}

static void ShroomUpdateBotInput(ShroomWorldState* world, ShroomPlayerState* bot,
                                 const ShroomSporeGridCell* spore_grid) {
  size_t index;
  const ShroomPlayerState* best_threat = 0;
  const ShroomPlayerState* best_prey = 0;
  const ShroomSporeState* best_spore = 0;
  const ShroomPowerupState* best_powerup = 0;
  float best_threat_score = 0.0f;
  float best_prey_score = 0.0f;
  float best_spore_score = 0.0f;
  float best_threat_distance = 0.0f;
  float best_prey_distance = 0.0f;
  float best_powerup_score = 0.0f;
  const ShroomBotRiskProfile profile = ShroomBotProfileForPlayer(bot->player_id);
  const ShroomZone current_zone = ShroomGetZoneAtPosition(world, bot->position);

  for (index = 0; index < world->player_count; ++index) {
    const ShroomPlayerState* other = &world->players[index];
    const float distance_sqr = ShroomDistanceSqr(bot->position, other->position);
    const float distance_bias = distance_sqr + 1.0f;

    if ((other == bot) || !other->alive || (other->player_id == bot->player_id)) {
      continue;
    }

    if (other->mass > (bot->mass * ShroomGetConsumeMassAdvantageAtPosition(world, bot->position))) {
      const float threat_score = (other->mass / bot->mass) * (1600000.0f / distance_bias);

      if ((best_threat == 0) || (threat_score > best_threat_score)) {
        best_threat = other;
        best_threat_score = threat_score;
        best_threat_distance = distance_sqr;
      }
    } else if (bot->mass >
               (other->mass * ShroomGetConsumeMassAdvantageAtPosition(world, other->position))) {
      const float prey_score = (bot->mass / other->mass) * (1100000.0f / distance_bias);

      if ((best_prey == 0) || (prey_score > best_prey_score)) {
        best_prey = other;
        best_prey_score = prey_score;
        best_prey_distance = distance_sqr;
      }
    }
  }

  for (index = 0; index < world->powerup_count; ++index) {
    const ShroomPowerupState* powerup = &world->powerups[index];
    const float distance_sqr = ShroomDistanceSqr(bot->position, powerup->position);
    float score;

    if (!powerup->active ||
        (distance_sqr > (SHROOM_BOT_POWERUP_SEARCH_RADIUS * SHROOM_BOT_POWERUP_SEARCH_RADIUS))) {
      continue;
    }
    score = ShroomBotPowerupUtility(bot, powerup, profile) * 1100000.0f / (distance_sqr + 1600.0f);
    if ((best_powerup == NULL) || (score > best_powerup_score)) {
      best_powerup = powerup;
      best_powerup_score = score;
    }
  }

  ShroomBotTryTacticalAction(world, bot, best_prey, best_prey_distance, best_threat,
                             best_threat_distance, best_powerup);

  if ((best_threat != 0) && (best_threat_distance < (1150.0f * 1150.0f))) {
    ShroomVec2 flee_direction = ShroomVec2Sub(bot->position, best_threat->position);

    if (current_zone != SHROOM_ZONE_OUTER) {
      flee_direction = ShroomVec2Add(
          flee_direction,
          ShroomVec2Scale(ShroomVec2Sub(bot->position, ShroomWorldCenter(world)), 0.45f));
    }

    bot->input_direction = ShroomNormalizeOrZero(flee_direction);
    return;
  }

  if ((world->game_mode == SHROOM_GAME_MODE_KING_OF_HILL) &&
      (profile == SHROOM_BOT_PROFILE_OBJECTIVE) && (current_zone != SHROOM_ZONE_CENTER)) {
    bot->input_direction =
        ShroomNormalizeOrZero(ShroomVec2Sub(ShroomWorldCenter(world), bot->position));
    return;
  }

  if ((best_powerup != NULL) && ((profile == SHROOM_BOT_PROFILE_OBJECTIVE) || (best_prey == NULL) ||
                                 (best_powerup_score > best_prey_score * 1.25f))) {
    bot->input_direction =
        ShroomNormalizeOrZero(ShroomVec2Sub(best_powerup->position, bot->position));
    return;
  }

  if ((best_prey != 0) && (best_prey_score > 1.2f)) {
    bot->input_direction = ShroomNormalizeOrZero(ShroomVec2Sub(best_prey->position, bot->position));
    return;
  }

  if (spore_grid != NULL) {
    int min_col =
        (int)((bot->position.x - SHROOM_BOT_SPORE_SEARCH_RADIUS) / SHROOM_SPORE_GRID_CELL_SIZE);
    int max_col =
        (int)((bot->position.x + SHROOM_BOT_SPORE_SEARCH_RADIUS) / SHROOM_SPORE_GRID_CELL_SIZE);
    int min_row =
        (int)((bot->position.y - SHROOM_BOT_SPORE_SEARCH_RADIUS) / SHROOM_SPORE_GRID_CELL_SIZE);
    int max_row =
        (int)((bot->position.y + SHROOM_BOT_SPORE_SEARCH_RADIUS) / SHROOM_SPORE_GRID_CELL_SIZE);
    int col, row;

    if (min_col < 0) {
      min_col = 0;
    }
    if (max_col >= SHROOM_SPORE_GRID_COLS) {
      max_col = SHROOM_SPORE_GRID_COLS - 1;
    }
    if (min_row < 0) {
      min_row = 0;
    }
    if (max_row >= SHROOM_SPORE_GRID_ROWS) {
      max_row = SHROOM_SPORE_GRID_ROWS - 1;
    }

    for (row = min_row; row <= max_row; ++row) {
      for (col = min_col; col <= max_col; ++col) {
        const ShroomSporeGridCell* cell = &spore_grid[row * SHROOM_SPORE_GRID_COLS + col];
        uint16_t i;

        for (i = 0; i < cell->count; ++i) {
          const ShroomSporeState* spore = &world->spores[cell->indices[i]];
          const float distance_sqr = ShroomDistanceSqr(bot->position, spore->position);
          const float weighted_distance =
              (distance_sqr + 1600.0f) /
              ShroomBotSporeZoneWeight(bot->mass, ShroomGetZoneAtPosition(world, spore->position));
          const float spore_score = 1.0f / weighted_distance;

          if (!spore->active) {
            continue;
          }

          if ((best_spore == 0) || (spore_score > best_spore_score)) {
            best_spore = spore;
            best_spore_score = spore_score;
          }
        }
      }
    }
  } else {
    for (index = 0; index < world->spore_count; ++index) {
      const ShroomSporeState* spore = &world->spores[index];
      const float distance_sqr = ShroomDistanceSqr(bot->position, spore->position);
      const float weighted_distance =
          (distance_sqr + 1600.0f) /
          ShroomBotSporeZoneWeight(bot->mass, ShroomGetZoneAtPosition(world, spore->position));
      const float spore_score = 1.0f / weighted_distance;

      if (!spore->active) {
        continue;
      }

      if ((best_spore == 0) || (spore_score > best_spore_score)) {
        best_spore = spore;
        best_spore_score = spore_score;
      }
    }
  }

  if ((bot->mass > (SHROOM_DEFAULT_PLAYER_MASS * 1.5f)) && (current_zone != SHROOM_ZONE_CENTER)) {
    const ShroomVec2 center_push =
        ShroomVec2Scale(ShroomVec2Sub(ShroomWorldCenter(world), bot->position),
                        ShroomBotCenterPressureWeight(bot->mass, current_zone));

    if ((best_spore == 0) || (best_spore_score < 0.00002f)) {
      bot->input_direction = ShroomNormalizeOrZero(center_push);
      return;
    }
  }

  if (best_spore != 0) {
    bot->input_direction =
        ShroomNormalizeOrZero(ShroomVec2Sub(best_spore->position, bot->position));
    return;
  }

  bot->input_direction =
      ShroomNormalizeOrZero(ShroomVec2Sub(ShroomWorldCenter(world), bot->position));
}

static void ShroomApplyMagnetToSpores(ShroomWorldState* world, const ShroomPlayerState* player,
                                      float delta_time) {
  const float magnet_radius_sqr = SHROOM_POWERUP_MAGNET_RADIUS * SHROOM_POWERUP_MAGNET_RADIUS;

  if ((player->magnet_powerup_timer <= 0.0f) || (delta_time <= 0.0f)) {
    return;
  }

  for (size_t index = 0; index < world->spore_count; ++index) {
    ShroomSporeState* spore = &world->spores[index];
    ShroomVec2 to_player;
    float distance_sqr;
    float distance;
    float step;

    if (!spore->active) {
      continue;
    }
    to_player = ShroomVec2Sub(player->position, spore->position);
    distance_sqr = ShroomVec2LengthSqr(to_player);
    if ((distance_sqr <= 0.0001f) || (distance_sqr > magnet_radius_sqr)) {
      continue;
    }
    distance = sqrtf(distance_sqr);
    step = fminf(distance, SHROOM_POWERUP_MAGNET_PULL_SPEED * delta_time);
    spore->position = ShroomVec2Add(spore->position, ShroomVec2Scale(to_player, step / distance));
  }
}

static void ShroomCollectSpores(ShroomWorldState* world, const ShroomSporeGridCell* grid,
                                float delta_time) {
  size_t player_index;

  for (player_index = 0; player_index < world->player_count; ++player_index) {
    ShroomPlayerState* player = &world->players[player_index];
    const float collection_radius = player->radius + 6.0f;
    int min_col, max_col, min_row, max_row, col, row;

    if (!player->alive) {
      continue;
    }

    ShroomApplyMagnetToSpores(world, player, delta_time);

    if (player->mass >= SHROOM_MAX_PLAYER_MASS) {
      continue;
    }

    min_col = (int)((player->position.x - collection_radius) / SHROOM_SPORE_GRID_CELL_SIZE);
    max_col = (int)((player->position.x + collection_radius) / SHROOM_SPORE_GRID_CELL_SIZE);
    min_row = (int)((player->position.y - collection_radius) / SHROOM_SPORE_GRID_CELL_SIZE);
    max_row = (int)((player->position.y + collection_radius) / SHROOM_SPORE_GRID_CELL_SIZE);

    if (min_col < 0)
      min_col = 0;
    if (max_col >= SHROOM_SPORE_GRID_COLS)
      max_col = SHROOM_SPORE_GRID_COLS - 1;
    if (min_row < 0)
      min_row = 0;
    if (max_row >= SHROOM_SPORE_GRID_ROWS)
      max_row = SHROOM_SPORE_GRID_ROWS - 1;

    for (row = min_row; row <= max_row; ++row) {
      for (col = min_col; col <= max_col; ++col) {
        const ShroomSporeGridCell* cell = &grid[row * SHROOM_SPORE_GRID_COLS + col];
        uint16_t i;

        for (i = 0; i < cell->count; ++i) {
          ShroomSporeState* spore = &world->spores[cell->indices[i]];

          if (!spore->active) {
            continue;
          }

          if (ShroomDistanceSqr(player->position, spore->position) <=
              (collection_radius * collection_radius)) {
            ShroomRoundStats* stats;

            player->mass =
                ShroomClamp(player->mass + (float)spore->value, 0.0f, SHROOM_MAX_PLAYER_MASS);
            ShroomSpawnOrResetSpore(world, spore);
            stats = ShroomWorldEnsureRoundStats(world, player->player_id);
            if (stats != NULL) {
              stats->spores_collected += 1u;
            }
          }
        }
      }
    }
  }
}

static float ShroomDecayMassThresholdAtPosition(const ShroomWorldState* world,
                                                ShroomVec2 position) {
  const ShroomZone zone = ShroomGetZoneAtPosition(world, position);

  if (zone == SHROOM_ZONE_CENTER) {
    return SHROOM_DEFAULT_PLAYER_MASS * 2.0f;
  }
  if (zone == SHROOM_ZONE_MID) {
    return SHROOM_DECAY_MASS_THRESHOLD;
  }
  return SHROOM_MAX_PLAYER_MASS;
}

static float ShroomDecayRateAtPosition(const ShroomWorldState* world, ShroomVec2 position) {
  const ShroomZone zone = ShroomGetZoneAtPosition(world, position);

  if (zone == SHROOM_ZONE_CENTER) {
    return SHROOM_DECAY_RATE_CENTER_PER_SECOND;
  }
  if (zone == SHROOM_ZONE_MID) {
    return SHROOM_DECAY_RATE_MID_PER_SECOND;
  }
  return SHROOM_DECAY_RATE_OUTER_PER_SECOND;
}

bool ShroomPlayerHasConsumeProtection(const ShroomPlayerState* player) {
  return (player != NULL) &&
         ((player->shield_powerup_timer > 0.0f) || (player->spawn_protection_timer > 0.0f));
}

bool ShroomPlayerCanConsume(const ShroomWorldState* world, const ShroomPlayerState* attacker,
                            const ShroomPlayerState* target) {
  float overlap_radius;
  float required_advantage;
  bool near_boundary = false;

  if ((world == NULL) || (attacker == NULL) || (target == NULL)) {
    return false;
  }

  overlap_radius = attacker->radius * 0.88f;
  required_advantage = ShroomGetConsumeMassAdvantageAtPosition(world, target->position);
  if (!attacker->alive || !target->alive) {
    return false;
  }
  /* Pieces of the same player cannot consume each other. */
  if (attacker->player_id == target->player_id) {
    return false;
  }
  if (ShroomPlayerHasConsumeProtection(target)) {
    return false;
  }
  if (attacker->mass < (target->mass * required_advantage)) {
    return false;
  }

  /* Check if either player is near a boundary */
  if (attacker->position.x < SHROOM_BOUNDARY_CONSUME_MARGIN ||
      attacker->position.x > (world->width - SHROOM_BOUNDARY_CONSUME_MARGIN) ||
      attacker->position.y < SHROOM_BOUNDARY_CONSUME_MARGIN ||
      attacker->position.y > (world->height - SHROOM_BOUNDARY_CONSUME_MARGIN) ||
      target->position.x < SHROOM_BOUNDARY_CONSUME_MARGIN ||
      target->position.x > (world->width - SHROOM_BOUNDARY_CONSUME_MARGIN) ||
      target->position.y < SHROOM_BOUNDARY_CONSUME_MARGIN ||
      target->position.y > (world->height - SHROOM_BOUNDARY_CONSUME_MARGIN)) {
    near_boundary = true;
  }

  /* Use more generous overlap check near boundaries */
  if (near_boundary) {
    overlap_radius = attacker->radius * 1.2f + target->radius * 0.5f;
  }

  return ShroomDistanceSqr(attacker->position, target->position) <=
         (overlap_radius * overlap_radius);
}

bool ShroomPlayerCanDecay(const ShroomWorldState* world, const ShroomPlayerState* player) {
  if ((world == NULL) || (player == NULL) || !player->alive) {
    return false;
  }
  if (player->decay_immune_powerup_timer > 0.0f) {
    return false;
  }

  return player->mass > ShroomDecayMassThresholdAtPosition(world, player->position);
}

static void ShroomResolveConsumes(ShroomWorldState* world) {
  size_t attacker_index;
  bool consumed[SHROOM_MAX_PLAYER_ENTITIES] = {0};
  size_t consumed_by[SHROOM_MAX_PLAYER_ENTITIES] = {0};

  for (attacker_index = 0; attacker_index < world->player_count; ++attacker_index) {
    size_t target_index;
    const ShroomPlayerState* attacker = &world->players[attacker_index];

    if (!attacker->alive || consumed[attacker_index]) {
      continue;
    }

    for (target_index = 0; target_index < world->player_count; ++target_index) {
      const ShroomPlayerState* target = &world->players[target_index];

      if ((attacker_index == target_index) || consumed[target_index]) {
        continue;
      }

      if (ShroomPlayerCanConsume(world, attacker, target)) {
        consumed[target_index] = true;
        consumed_by[target_index] = attacker_index;
      }
    }
  }

  for (attacker_index = 0; attacker_index < world->player_count; ++attacker_index) {
    if (consumed[attacker_index]) {
      ShroomPlayerState* victim = &world->players[attacker_index];
      ShroomPlayerState* winner = &world->players[consumed_by[attacker_index]];
      const ShroomPlayerId victim_player_id = victim->player_id;
      ShroomRoundStats* stats;

      winner->mass = ShroomClamp(winner->mass + (victim->mass * SHROOM_CONSUME_MASS_GAIN_FACTOR),
                                 0.0f, SHROOM_MAX_PLAYER_MASS);
      victim->alive = false;
      stats = ShroomWorldEnsureRoundStats(world, winner->player_id);
      if (stats != NULL) {
        stats->kills += 1u;
      }
      if (victim->piece_index == 0) {
        /* Primary piece consumed: respawn the player and remove any orphaned split pieces. */
        ShroomRespawnPlayer(world, victim);
        {
          size_t ki;
          for (ki = 0; ki < world->player_count; ++ki) {
            ShroomPlayerState* p = &world->players[ki];
            if ((p != victim) && p->alive && (p->player_id == victim_player_id) &&
                (p->piece_index > 0)) {
              *p = (ShroomPlayerState){0};
            }
          }
        }
      } else {
        /* Split fragment consumed: clear the slot without respawning. */
        victim->mass = 0.0f;
        victim->radius = 0.0f;
        victim->split_velocity = (ShroomVec2){0};
        victim->merge_timer = 0.0f;
        victim->ai_controlled = false;
        victim->piece_index = 0;
        victim->player_id = 0;
      }
    }
  }
}

float ShroomMassToRadius(float mass) { return 10.0f + (mass * 0.14f); }

float ShroomMassToSpeed(float mass) {
  const float decay_threshold = SHROOM_DEFAULT_PLAYER_MASS * 8.0f;
  const float floor_speed = SHROOM_MIN_PLAYER_SPEED * SHROOM_SPEED_FLOOR_FACTOR;
  const float scaled_mass = mass * SHROOM_PLAYER_SPEED_MASS_SCALE;
  const float speed = SHROOM_MAX_PLAYER_SPEED / (1.0f + (scaled_mass / 100.0f));
  const float clamped_base_speed =
      ShroomClamp(speed, SHROOM_MIN_PLAYER_SPEED, SHROOM_MAX_PLAYER_SPEED);

  if (mass <= decay_threshold) {
    return clamped_base_speed;
  }

  if (SHROOM_MAX_PLAYER_MASS <= decay_threshold) {
    return floor_speed;
  }

  return ShroomClamp(
      SHROOM_MIN_PLAYER_SPEED +
          ((floor_speed - SHROOM_MIN_PLAYER_SPEED) *
           ShroomClamp((mass - decay_threshold) / (SHROOM_MAX_PLAYER_MASS - decay_threshold), 0.0f,
                       1.0f)),
      floor_speed, SHROOM_MIN_PLAYER_SPEED);
}

static void ShroomApplyMassLoss(ShroomWorldState* world, ShroomPlayerState* player,
                                float mass_loss) {
  float eject_radius;
  float angle;
  ShroomVec2 spore_position;
  uint16_t spore_value;

  if (mass_loss <= 0.0f) {
    return;
  }

  eject_radius = player->radius + 12.0f;
  angle = ShroomRandomFloat(world, 0.0f, 6.2831853f);
  spore_position = (ShroomVec2){
      ShroomClamp(player->position.x + (cosf(angle) * eject_radius), 60.0f, world->width - 60.0f),
      ShroomClamp(player->position.y + (sinf(angle) * eject_radius), 60.0f, world->height - 60.0f),
  };

  player->mass = ShroomClamp(player->mass - mass_loss, 0.0f, SHROOM_MAX_PLAYER_MASS);
  player->decay_spore_accumulator += mass_loss;
  spore_value = (uint16_t)player->decay_spore_accumulator;
  if (spore_value > 0u) {
    ShroomSpawnDecaySpore(world, spore_position, spore_value);
    player->decay_spore_accumulator -= (float)spore_value;
  }
}

static bool ShroomPlayerIdlePenaltyActive(const ShroomPlayerState* player,
                                          uint64_t current_time_ms) {
  const uint64_t grace_ms = (uint64_t)(SHROOM_IDLE_PENALTY_GRACE_SECONDS * 1000.0f);

  if ((player == NULL) || !player->alive) {
    return false;
  }
  return current_time_ms > player->last_move_time_ms + grace_ms;
}

static void ShroomApplyMassRules(ShroomWorldState* world, float delta_time,
                                 uint64_t current_time_ms) {
  size_t index;

  for (index = 0; index < world->player_count; ++index) {
    ShroomPlayerState* player = &world->players[index];

    if (!player->alive) {
      continue;
    }

    player->mass = ShroomClamp(player->mass, 0.0f, SHROOM_MAX_PLAYER_MASS);
    if (ShroomPlayerCanDecay(world, player)) {
      const float decay_threshold = ShroomDecayMassThresholdAtPosition(world, player->position);
      const float excess_mass = player->mass - decay_threshold;
      const float decay_mass =
          excess_mass * ShroomDecayRateAtPosition(world, player->position) * delta_time;
      ShroomApplyMassLoss(world, player, decay_mass);
    }
    if ((player->decay_immune_powerup_timer <= 0.0f) &&
        ShroomPlayerIdlePenaltyActive(player, current_time_ms)) {
      ShroomApplyMassLoss(world, player, SHROOM_IDLE_PENALTY_BLEED_PER_SECOND * delta_time);
    }

    player->radius = ShroomMassToRadius(player->mass);
  }
}

void ShroomWorldInitWithSeed(ShroomWorldState* world, uint32_t seed) {
  *world = (ShroomWorldState){0};
  world->width = SHROOM_WORLD_WIDTH;
  world->height = SHROOM_WORLD_HEIGHT;
  world->random_seed = ShroomNormalizeSeed(seed);
  world->random_state = world->random_seed;
  world->next_entity_id = 1;
  world->match_phase = SHROOM_MATCH_PHASE_RUNNING;
  world->match_duration_seconds = SHROOM_MATCH_DURATION_SECONDS;
  world->match_time_remaining = SHROOM_MATCH_DURATION_SECONDS;
  world->match_results_time_remaining = 0.0f;
  world->game_mode = SHROOM_GAME_MODE_FFA;
  world->objective_target_score = SHROOM_KOTH_TARGET_SCORE;
  ShroomInitializeSpores(world);
  ShroomInitializePowerups(world);
}

void ShroomWorldInit(ShroomWorldState* world) {
  uint32_t seed;

#if defined(_WIN32)
  seed = (uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)world;
#else
  struct timespec seed_time = {0};
  timespec_get(&seed_time, TIME_UTC);
  seed = (uint32_t)seed_time.tv_sec ^ (uint32_t)seed_time.tv_nsec ^ (uint32_t)(uintptr_t)world;
#endif
  ShroomWorldInitWithSeed(world, seed);
}

const ShroomRoundStats* ShroomWorldGetRoundStats(const ShroomWorldState* world,
                                                 ShroomPlayerId player_id) {
  size_t index;

  if ((world == NULL) || (player_id == 0u)) {
    return NULL;
  }
  for (index = 0; index < SHROOM_MAX_PARTICIPANTS; ++index) {
    if (world->round_stats[index].player_id == player_id) {
      return &world->round_stats[index].stats;
    }
  }
  return NULL;
}

static ShroomRoundStats* ShroomWorldEnsureRoundStats(ShroomWorldState* world,
                                                     ShroomPlayerId player_id) {
  size_t index;

  if ((world == NULL) || (player_id == 0u)) {
    return NULL;
  }
  for (index = 0; index < SHROOM_MAX_PARTICIPANTS; ++index) {
    if (world->round_stats[index].player_id == player_id) {
      return &world->round_stats[index].stats;
    }
  }
  for (index = 0; index < SHROOM_MAX_PARTICIPANTS; ++index) {
    if (world->round_stats[index].player_id == 0u) {
      world->round_stats[index].player_id = player_id;
      world->round_stats[index].stats = (ShroomRoundStats){0};
      return &world->round_stats[index].stats;
    }
  }
  return NULL;
}

ShroomPlayerState* ShroomWorldSpawnPlayer(ShroomWorldState* world, ShroomPlayerId player_id,
                                          bool is_bot) {
  ShroomPlayerState* player = NULL;
  size_t index;

  if ((world == NULL) || (player_id == 0u)) {
    return NULL;
  }

  for (index = 0; index < world->player_count; ++index) {
    if (!world->players[index].alive) {
      player = &world->players[index];
      break;
    }
  }

  if ((player == NULL) && (world->player_count >= SHROOM_MAX_PLAYER_ENTITIES)) {
    return NULL;
  }
  (void)ShroomWorldEnsureRoundStats(world, player_id);

  if (player == NULL) {
    player = &world->players[world->player_count++];
  }

  *player = (ShroomPlayerState){
      .player_id = player_id,
      .entity_id = world->next_entity_id++,
      .position = ShroomRandomSpawnPosition(world, true),
      .mass = SHROOM_DEFAULT_PLAYER_MASS,
      .radius = ShroomMassToRadius(SHROOM_DEFAULT_PLAYER_MASS),
      .last_move_time_ms = ShroomWorldCurrentTimeMs(world),
      .alive = true,
      .is_bot = is_bot,
      .spawn_protection_timer = is_bot ? 0.0f : SHROOM_PLAYER_SPAWN_PROTECTION_SECONDS,
  };

  return player;
}

size_t ShroomWorldRemovePlayer(ShroomWorldState* world, ShroomPlayerId player_id) {
  size_t removed = 0;
  size_t i;

  if ((world == NULL) || (player_id == 0u)) {
    return 0;
  }

  for (i = 0; i < world->player_count; ++i) {
    if (world->players[i].player_id == player_id) {
      world->players[i] = (ShroomPlayerState){0};
      ++removed;
    }
  }

  while ((world->player_count > 0u) && (world->players[world->player_count - 1u].player_id == 0u)) {
    --world->player_count;
  }
  if (removed > 0u) {
    for (i = 0; i < SHROOM_MAX_PARTICIPANTS; ++i) {
      if (world->objective_player_ids[i] == player_id) {
        world->objective_player_ids[i] = 0u;
        world->objective_scores[i] = 0.0f;
        break;
      }
    }
    for (i = 0; i < SHROOM_MAX_PARTICIPANTS; ++i) {
      if (world->round_stats[i].player_id == player_id) {
        world->round_stats[i] = (ShroomRoundStatsSlot){0};
        break;
      }
    }
  }

  return removed;
}

static void ShroomCollectPowerups(ShroomWorldState* world) {
  for (size_t player_index = 0; player_index < world->player_count; ++player_index) {
    ShroomPlayerState* player = &world->players[player_index];

    if (!player->alive) {
      continue;
    }

    for (size_t powerup_index = 0; powerup_index < world->powerup_count; ++powerup_index) {
      ShroomPowerupState* powerup = &world->powerups[powerup_index];
      const float collection_radius = player->radius + SHROOM_POWERUP_RADIUS;

      if (!powerup->active) {
        continue;
      }
      if (ShroomDistanceSqr(player->position, powerup->position) >
          (collection_radius * collection_radius)) {
        continue;
      }

      switch (powerup->type) {
      case SHROOM_POWERUP_SPEED:
        player->speed_powerup_timer = SHROOM_POWERUP_SPEED_SECONDS;
        break;
      case SHROOM_POWERUP_SHIELD:
        player->shield_powerup_timer = SHROOM_POWERUP_SHIELD_SECONDS;
        break;
      case SHROOM_POWERUP_MAGNET:
        player->magnet_powerup_timer = SHROOM_POWERUP_MAGNET_SECONDS;
        break;
      case SHROOM_POWERUP_DECAY_IMMUNE:
        player->decay_immune_powerup_timer = SHROOM_POWERUP_DECAY_IMMUNE_SECONDS;
        break;
      default:
        break;
      }

      powerup->active = false;
      powerup->respawn_timer = SHROOM_POWERUP_RESPAWN_SECONDS;
    }
  }
}

static void ShroomUpdatePowerups(ShroomWorldState* world, float delta_time) {
  for (size_t index = 0; index < world->powerup_count; ++index) {
    ShroomPowerupState* powerup = &world->powerups[index];

    if (powerup->active || (powerup->respawn_timer <= 0.0f)) {
      continue;
    }

    powerup->respawn_timer -= delta_time;
    if (powerup->respawn_timer <= 0.0f) {
      ShroomSpawnOrResetPowerup(world, powerup, index);
    }
  }
}

static void ShroomUpdatePlayerEffects(ShroomWorldState* world, float delta_time) {
  for (size_t index = 0; index < world->player_count; ++index) {
    ShroomPlayerState* player = &world->players[index];

    if (!player->alive) {
      continue;
    }
    if (player->speed_powerup_timer > 0.0f) {
      player->speed_powerup_timer =
          ShroomClamp(player->speed_powerup_timer - delta_time, 0.0f, SHROOM_POWERUP_SPEED_SECONDS);
    }
    if (player->shield_powerup_timer > 0.0f) {
      player->shield_powerup_timer = ShroomClamp(player->shield_powerup_timer - delta_time, 0.0f,
                                                 SHROOM_POWERUP_SHIELD_SECONDS);
    }
    if (player->magnet_powerup_timer > 0.0f) {
      player->magnet_powerup_timer = ShroomClamp(player->magnet_powerup_timer - delta_time, 0.0f,
                                                 SHROOM_POWERUP_MAGNET_SECONDS);
    }
    if (player->decay_immune_powerup_timer > 0.0f) {
      player->decay_immune_powerup_timer =
          ShroomClamp(player->decay_immune_powerup_timer - delta_time, 0.0f,
                      SHROOM_POWERUP_DECAY_IMMUNE_SECONDS);
    }
  }
}

/* ---------------------------------------------------------------------------
 * Split / merge helpers
 * ---------------------------------------------------------------------- */

static int ShroomCountPlayerPieces(const ShroomWorldState* world, ShroomPlayerId player_id) {
  int count = 0;
  size_t i;
  for (i = 0; i < world->player_count; ++i) {
    if (world->players[i].alive && (world->players[i].player_id == player_id)) {
      ++count;
    }
  }
  return count;
}

bool ShroomPlayerCanSplit(const ShroomWorldState* world, const ShroomPlayerState* player) {
  if ((world == NULL) || (player == NULL) || !player->alive) {
    return false;
  }
  /* ## Decisions: split mass floor.
   * The floor is checked against pre-cost mass so a 4x default-mass colony can
   * split tactically. The cost is still paid before halving, making the split a
   * commitment without reserving the mechanic only for near-cap players. */
  if (player->mass < SHROOM_SPLIT_MIN_MASS) {
    return false;
  }
  if (!player->is_bot && player->has_split) {
    return false;
  }
  return ShroomCountPlayerPieces(world, player->player_id) < SHROOM_MAX_SPLIT_PIECES;
}

static ShroomPlayerState* ShroomFindPrimaryPiece(ShroomWorldState* world,
                                                 ShroomPlayerId player_id) {
  size_t i;
  for (i = 0; i < world->player_count; ++i) {
    ShroomPlayerState* p = &world->players[i];
    if (p->alive && (p->player_id == player_id) && (p->piece_index == 0)) {
      return p;
    }
  }
  return NULL;
}

bool ShroomWorldSplitPlayerToward(ShroomWorldState* world, ShroomPlayerState* player,
                                  ShroomVec2 aim_direction) {
  ShroomPlayerState* new_piece;
  float split_cost;
  float half_mass;
  ShroomVec2 launch_dir;
  int piece_count;
  size_t i;

  if (!ShroomPlayerCanSplit(world, player)) {
    return false;
  }
  piece_count = ShroomCountPlayerPieces(world, player->player_id);

  /* Find a free slot (zeroed-out merged/fragment slot) or extend the array. */
  new_piece = NULL;
  for (i = 0; i < world->player_count; ++i) {
    if (!world->players[i].alive && (world->players[i].player_id == 0)) {
      new_piece = &world->players[i];
      break;
    }
  }
  if (new_piece == NULL) {
    if (world->player_count >= SHROOM_MAX_PLAYER_ENTITIES) {
      return false;
    }
    new_piece = &world->players[world->player_count++];
  }

  /* ## Decisions: split mass cost.
   * The split-cost branch deducts SHROOM_SPLIT_MASS_LOSS_FRACTION of the
   * parent's mass BEFORE halving. The lost mass is discarded — it is not
   * spawned as a decay spore, because doing so would feed the local player
   * free recapture of their own ejecta. Discarding keeps split a real
   * tactical tradeoff, not a free lunch. */
  split_cost = player->mass * SHROOM_SPLIT_MASS_LOSS_FRACTION;
  half_mass = (player->mass - split_cost) / 2.0f;
  launch_dir = ShroomNormalizeOrZero(aim_direction);
  if (ShroomVec2LengthSqr(launch_dir) < 0.0001f) {
    launch_dir = ShroomNormalizeOrZero(player->input_direction);
  }
  if (ShroomVec2LengthSqr(launch_dir) < 0.0001f) {
    launch_dir = (ShroomVec2){1.0f, 0.0f};
  }

  *new_piece = (ShroomPlayerState){
      .player_id = player->player_id,
      .entity_id = world->next_entity_id++,
      .position = player->position,
      .input_direction = launch_dir,
      .mass = half_mass,
      .radius = ShroomMassToRadius(half_mass),
      .last_move_time_ms = ShroomWorldCurrentTimeMs(world),
      .alive = true,
      .is_bot = player->is_bot,
      .ai_controlled = player->is_bot,
      .merge_timer = SHROOM_SPLIT_MERGE_SECONDS,
      .spawn_protection_timer = SHROOM_SPLIT_PROTECTION_SECONDS,
      .piece_index = (uint8_t)piece_count,
      .split_velocity = ShroomVec2Scale(launch_dir, SHROOM_SPLIT_IMPULSE_SPEED),
  };
  snprintf(new_piece->name, sizeof(new_piece->name), "%s", player->name);

  player->mass = half_mass;
  player->radius = ShroomMassToRadius(half_mass);
  player->last_move_time_ms = ShroomWorldCurrentTimeMs(world);
  player->has_split = true;
  if (player->merge_timer <= 0.0f) {
    player->merge_timer = SHROOM_SPLIT_MERGE_SECONDS;
  }
  player->spawn_protection_timer = SHROOM_SPLIT_PROTECTION_SECONDS;

  return true;
}

bool ShroomWorldSplitPlayer(ShroomWorldState* world, ShroomPlayerState* player) {
  return ShroomWorldSplitPlayerToward(world, player,
                                      player != NULL ? player->input_direction : (ShroomVec2){0});
}

bool ShroomWorldEjectMass(ShroomWorldState* world, ShroomPlayerState* player,
                          ShroomVec2 aim_direction) {
  ShroomVec2 eject_dir;
  ShroomVec2 eject_position;
  float mass_loss;

  if ((world == NULL) || (player == NULL) || !player->alive) {
    return false;
  }
  if ((player->eject_cooldown_timer > 0.0f) || (player->mass < SHROOM_EJECT_MIN_MASS)) {
    return false;
  }

  mass_loss = SHROOM_EJECT_MASS_VALUE * (1.0f + SHROOM_EJECT_COST_FRACTION);
  if ((player->mass - mass_loss) < SHROOM_DEFAULT_PLAYER_MASS) {
    return false;
  }

  eject_dir = ShroomNormalizeOrZero(aim_direction);
  if (ShroomVec2LengthSqr(eject_dir) < 0.0001f) {
    eject_dir = ShroomNormalizeOrZero(player->input_direction);
  }
  if (ShroomVec2LengthSqr(eject_dir) < 0.0001f) {
    eject_dir = (ShroomVec2){1.0f, 0.0f};
  }

  eject_position = ShroomVec2Add(player->position,
                                 ShroomVec2Scale(eject_dir, player->radius + SHROOM_POWERUP_RADIUS +
                                                                SHROOM_EJECT_IMPULSE_RANGE));
  eject_position.x =
      ShroomClamp(eject_position.x, SHROOM_POWERUP_RADIUS, world->width - SHROOM_POWERUP_RADIUS);
  eject_position.y =
      ShroomClamp(eject_position.y, SHROOM_POWERUP_RADIUS, world->height - SHROOM_POWERUP_RADIUS);

  player->mass -= mass_loss;
  player->radius = ShroomMassToRadius(player->mass);
  player->eject_cooldown_timer = SHROOM_EJECT_COOLDOWN_SECONDS;
  ShroomSpawnDecaySpore(world, eject_position, (uint16_t)SHROOM_EJECT_MASS_VALUE);
  return true;
}

bool ShroomPlayersCanMerge(const ShroomPlayerState* primary, const ShroomPlayerState* piece) {
  float merge_radius;

  if ((primary == NULL) || (piece == NULL) || !primary->alive || !piece->alive) {
    return false;
  }
  if ((primary->player_id != piece->player_id) || (primary->piece_index != 0) ||
      (piece->piece_index == 0)) {
    return false;
  }
  if ((primary->merge_timer > 0.0f) || (piece->merge_timer > 0.0f)) {
    return false;
  }

  merge_radius = primary->radius + piece->radius;
  return ShroomDistanceSqr(primary->position, piece->position) <= (merge_radius * merge_radius);
}

static void ShroomResolveMerges(ShroomWorldState* world) {
  size_t i;
  for (i = 0; i < world->player_count; ++i) {
    ShroomPlayerState* piece = &world->players[i];
    ShroomPlayerState* primary;

    if (!piece->alive || (piece->piece_index == 0)) {
      continue;
    }
    primary = ShroomFindPrimaryPiece(world, piece->player_id);
    if (!ShroomPlayersCanMerge(primary, piece)) {
      continue;
    }
    primary->mass = ShroomClamp(primary->mass + piece->mass, 0.0f, SHROOM_MAX_PLAYER_MASS);
    primary->radius = ShroomMassToRadius(primary->mass);
    /* Zero the slot so it can be reused for future split pieces. */
    *piece = (ShroomPlayerState){0};
  }
}

static void ShroomApplyForcedSplits(ShroomWorldState* world) {
  const size_t initial_count = world->player_count;
  size_t i;

  for (i = 0; i < initial_count; ++i) {
    ShroomPlayerState* player = &world->players[i];

    /* Autosplit applies to bots only; players choose when to split. */
    if (!player->is_bot) {
      continue;
    }
    if (!ShroomPlayerCanSplit(world, player) || (player->mass < SHROOM_MAX_PLAYER_MASS)) {
      continue;
    }
    ShroomWorldSplitPlayer(world, player);
  }
}

void ShroomPlayerSetInput(ShroomPlayerState* player, ShroomVec2 input_direction) {
  if (player == 0) {
    return;
  }

  player->input_direction = input_direction;
}

float ShroomWorldGetColonyMass(const ShroomWorldState* world, ShroomPlayerId player_id) {
  float total_mass = 0.0f;

  if ((world == NULL) || (player_id == 0)) {
    return 0.0f;
  }
  for (size_t index = 0; index < world->player_count; ++index) {
    const ShroomPlayerState* piece = &world->players[index];
    if (piece->alive && (piece->player_id == player_id) && (piece->mass > 0.0f)) {
      total_mass += piece->mass;
    }
  }
  return total_mass;
}

void ShroomComputeMatchPodium(ShroomWorldState* world) {
  size_t i;
  size_t j;

  if (world == NULL) {
    return;
  }

  for (i = 0; i < SHROOM_MATCH_PODIUM_COUNT; ++i) {
    world->podium_player_ids[i] = 0;
    world->podium_masses[i] = 0.0f;
  }

  for (i = 0; i < world->player_count; ++i) {
    const ShroomPlayerState* player = &world->players[i];
    float colony_mass;
    bool already_scored = false;

    if (!player->alive || (player->mass <= 0.0f)) {
      continue;
    }
    for (size_t previous = 0; previous < i; ++previous) {
      if (world->players[previous].alive &&
          (world->players[previous].player_id == player->player_id)) {
        already_scored = true;
        break;
      }
    }
    if (already_scored) {
      continue;
    }
    colony_mass = world->game_mode == SHROOM_GAME_MODE_KING_OF_HILL
                      ? ShroomWorldGetObjectiveScore(world, player->player_id)
                      : ShroomWorldGetColonyMass(world, player->player_id);

    for (j = 0; j < SHROOM_MATCH_PODIUM_COUNT; ++j) {
      const float candidate_tiebreak_mass = ShroomWorldGetColonyMass(world, player->player_id);
      const float placed_tiebreak_mass =
          ShroomWorldGetColonyMass(world, world->podium_player_ids[j]);
      if ((colony_mass > world->podium_masses[j]) ||
          ((colony_mass == world->podium_masses[j]) &&
           ((world->podium_player_ids[j] == 0) ||
            ((world->game_mode == SHROOM_GAME_MODE_KING_OF_HILL) &&
             ((candidate_tiebreak_mass > placed_tiebreak_mass) ||
              ((candidate_tiebreak_mass == placed_tiebreak_mass) &&
               (player->player_id < world->podium_player_ids[j])))) ||
            ((world->game_mode != SHROOM_GAME_MODE_KING_OF_HILL) &&
             (player->player_id < world->podium_player_ids[j]))))) {
        size_t k;
        for (k = SHROOM_MATCH_PODIUM_COUNT - 1; k > j; --k) {
          world->podium_player_ids[k] = world->podium_player_ids[k - 1];
          world->podium_masses[k] = world->podium_masses[k - 1];
        }
        world->podium_player_ids[j] = player->player_id;
        world->podium_masses[j] = colony_mass;
        break;
      }
    }
  }
}

void ShroomWorldSetGameMode(ShroomWorldState* world, ShroomGameMode game_mode) {
  if ((world == NULL) || (game_mode < SHROOM_GAME_MODE_FFA) ||
      (game_mode >= SHROOM_GAME_MODE_COUNT)) {
    return;
  }
  world->game_mode = game_mode;
  world->objective_target_score = SHROOM_KOTH_TARGET_SCORE;
  world->objective_controller_id = 0u;
  world->objective_contested = false;
  memset(world->objective_player_ids, 0, sizeof(world->objective_player_ids));
  memset(world->objective_scores, 0, sizeof(world->objective_scores));
}

void ShroomWorldSetMatchDuration(ShroomWorldState* world, float duration_seconds) {
  if (world == NULL) {
    return;
  }
  if (duration_seconds < 1.0f) {
    duration_seconds = 1.0f;
  }
  world->match_duration_seconds = duration_seconds;
  world->match_time_remaining = duration_seconds;
}

void ShroomWorldResetMatch(ShroomWorldState* world) {
  size_t i;

  if (world == NULL) {
    return;
  }

  for (i = 0; i < world->player_count; ++i) {
    ShroomPlayerState* player = &world->players[i];

    if (player->alive) {
      bool primary_already_seen = false;

      for (size_t previous = 0; previous < i; ++previous) {
        const ShroomPlayerState* candidate = &world->players[previous];
        if (candidate->alive && (candidate->player_id == player->player_id) &&
            (candidate->piece_index == 0)) {
          primary_already_seen = true;
          break;
        }
      }
      if ((player->piece_index != 0) || primary_already_seen) {
        *player = (ShroomPlayerState){0};
        continue;
      }

      player->position = ShroomRandomSpawnPosition(world, true);
      player->mass = SHROOM_DEFAULT_PLAYER_MASS;
      player->radius = ShroomMassToRadius(player->mass);
      player->input_direction = (ShroomVec2){0};
      player->split_velocity = (ShroomVec2){0};
      player->ai_controlled = false;
      player->has_split = false;
      player->piece_index = 0;
      player->merge_timer = 0.0f;
      player->spawn_protection_timer =
          player->is_bot ? 0.0f : SHROOM_PLAYER_SPAWN_PROTECTION_SECONDS;
      player->speed_powerup_timer = 0.0f;
      player->shield_powerup_timer = 0.0f;
      player->magnet_powerup_timer = 0.0f;
      player->decay_immune_powerup_timer = 0.0f;
      player->eject_cooldown_timer = 0.0f;
      player->decay_spore_accumulator = 0.0f;
      player->last_move_time_ms = ShroomWorldCurrentTimeMs(world);
    }
  }

  world->spore_count = 0;
  world->powerup_count = 0;
  ShroomInitializeSpores(world);
  ShroomInitializePowerups(world);

  for (i = 0; i < SHROOM_MATCH_PODIUM_COUNT; ++i) {
    world->podium_player_ids[i] = 0;
    world->podium_masses[i] = 0.0f;
  }

  memset(world->round_stats, 0, sizeof(world->round_stats));

  world->match_phase = SHROOM_MATCH_PHASE_RUNNING;
  world->match_time_remaining = world->match_duration_seconds;
  world->match_results_time_remaining = 0.0f;
  world->objective_controller_id = 0u;
  world->objective_contested = false;
  memset(world->objective_player_ids, 0, sizeof(world->objective_player_ids));
  memset(world->objective_scores, 0, sizeof(world->objective_scores));
}

float ShroomWorldGetObjectiveScore(const ShroomWorldState* world, ShroomPlayerId player_id) {
  if ((world == NULL) || (player_id == 0u)) {
    return 0.0f;
  }
  for (size_t index = 0; index < SHROOM_MAX_PARTICIPANTS; ++index) {
    if (world->objective_player_ids[index] == player_id) {
      return world->objective_scores[index];
    }
  }
  return 0.0f;
}

bool ShroomWorldSetObjectiveScore(ShroomWorldState* world, ShroomPlayerId player_id, float score) {
  size_t free_slot = SHROOM_MAX_PARTICIPANTS;

  if ((world == NULL) || (player_id == 0u)) {
    return false;
  }
  for (size_t index = 0; index < SHROOM_MAX_PARTICIPANTS; ++index) {
    if (world->objective_player_ids[index] == player_id) {
      world->objective_scores[index] = score;
      return true;
    }
    if ((free_slot == SHROOM_MAX_PARTICIPANTS) && (world->objective_player_ids[index] == 0u)) {
      free_slot = index;
    }
  }
  if (free_slot == SHROOM_MAX_PARTICIPANTS) {
    return false;
  }
  world->objective_player_ids[free_slot] = player_id;
  world->objective_scores[free_slot] = score;
  return true;
}

static bool ShroomUpdateKingOfHill(ShroomWorldState* world, float delta_time) {
  ShroomPlayerId present[SHROOM_MAX_PARTICIPANTS] = {0};
  ShroomPlayerId controller = 0u;
  size_t colony_count = 0u;

  if (world->game_mode != SHROOM_GAME_MODE_KING_OF_HILL) {
    world->objective_controller_id = 0u;
    world->objective_contested = false;
    return false;
  }

  for (size_t index = 0; index < world->player_count; ++index) {
    const ShroomPlayerState* piece = &world->players[index];
    if (!piece->alive || (piece->mass <= 0.0f) || (piece->spawn_protection_timer > 0.0f) ||
        (piece->player_id == 0u) ||
        (ShroomGetZoneAtPosition(world, piece->position) != SHROOM_ZONE_CENTER)) {
      continue;
    }
    bool seen = false;
    for (size_t colony = 0; colony < colony_count; ++colony) {
      if (present[colony] == piece->player_id) {
        seen = true;
        break;
      }
    }
    if (!seen && (colony_count < SHROOM_MAX_PARTICIPANTS)) {
      present[colony_count++] = piece->player_id;
    }
  }

  if (colony_count == 1u) {
    controller = present[0];
  }

  world->objective_contested = colony_count > 1u;
  world->objective_controller_id = colony_count == 1u ? controller : 0u;
  if (world->objective_controller_id == 0u) {
    return false;
  }

  float score =
      ShroomWorldGetObjectiveScore(world, controller) + (SHROOM_KOTH_SCORE_PER_SECOND * delta_time);
  if (score >= world->objective_target_score) {
    ShroomWorldSetObjectiveScore(world, controller, world->objective_target_score);
    return true;
  }
  ShroomWorldSetObjectiveScore(world, controller, score);
  return false;
}

static const ShroomPlayerState* ShroomFindStatsRepresentative(const ShroomWorldState* world,
                                                              ShroomPlayerId player_id) {
  const ShroomPlayerState* representative = NULL;
  size_t index;

  for (index = 0; index < world->player_count; ++index) {
    const ShroomPlayerState* player = &world->players[index];

    if (!player->alive || (player->player_id != player_id)) {
      continue;
    }
    if (player->piece_index == 0u) {
      return player;
    }
    if ((representative == NULL) || (player->piece_index < representative->piece_index) ||
        ((player->piece_index == representative->piece_index) &&
         (player->entity_id < representative->entity_id))) {
      representative = player;
    }
  }
  return representative;
}

static void ShroomUpdateColonyRoundStats(ShroomWorldState* world, float delta_time) {
  size_t index;

  for (index = 0; index < world->player_count; ++index) {
    const ShroomPlayerState* player = &world->players[index];

    if (player->alive) {
      (void)ShroomWorldEnsureRoundStats(world, player->player_id);
    }
  }

  for (index = 0; index < SHROOM_MAX_PARTICIPANTS; ++index) {
    ShroomRoundStatsSlot* slot = &world->round_stats[index];
    const ShroomPlayerState* representative;

    if (slot->player_id == 0u) {
      continue;
    }
    representative = ShroomFindStatsRepresentative(world, slot->player_id);
    if (representative == NULL) {
      slot->stats.colony_mass = 0.0f;
      continue;
    }

    slot->stats.colony_mass = ShroomWorldGetColonyMass(world, slot->player_id);
    if (slot->stats.colony_mass > slot->stats.peak_mass) {
      slot->stats.peak_mass = slot->stats.colony_mass;
    }
    slot->stats.survival_seconds += delta_time;

    /* A split colony occupies one zone per tick, attributed to its primary piece. */
    switch (ShroomGetZoneAtPosition(world, representative->position)) {
    case SHROOM_ZONE_CENTER:
      slot->stats.center_zone_seconds += delta_time;
      break;
    case SHROOM_ZONE_MID:
      slot->stats.mid_zone_seconds += delta_time;
      break;
    case SHROOM_ZONE_OUTER:
      slot->stats.outer_zone_seconds += delta_time;
      break;
    }
  }
}

void ShroomWorldStep(ShroomWorldState* world, float delta_time) {
  size_t index;
  ShroomSporeGridCell spore_grid[SHROOM_SPORE_GRID_CELLS];
  const uint64_t step_end_time_ms = ShroomWorldStepEndTimeMs(world, delta_time);

  /* Preserve the completed world and podium throughout the intermission. */
  if (world->match_phase != SHROOM_MATCH_PHASE_RUNNING) {
    if (world->match_phase == SHROOM_MATCH_PHASE_RESULTS) {
      world->match_results_time_remaining -= delta_time;
      if (world->match_results_time_remaining <= 0.0f) {
        world->match_results_time_remaining = 0.0f;
        world->match_phase = SHROOM_MATCH_PHASE_RESET;
      }
    }
    world->tick += 1;
    return;
  }

  ShroomBuildSporeGrid(world, spore_grid);

  for (index = 0; index < world->player_count; ++index) {
    ShroomPlayerState* player = &world->players[index];

    if ((player->is_bot || player->ai_controlled) && player->alive) {
      ShroomUpdateBotInput(world, player, spore_grid);
    }
  }

  for (index = 0; index < world->player_count; ++index) {
    ShroomPlayerState* player = &world->players[index];
    float speed = ShroomMassToSpeed(player->mass);
    ShroomVec2 velocity;

    if (!player->alive) {
      continue;
    }

    if (ShroomVec2LengthSqr(player->input_direction) > SHROOM_IDLE_PENALTY_MOVEMENT_THRESHOLD_SQR) {
      player->last_move_time_ms = step_end_time_ms;
    }

    if (player->speed_powerup_timer > 0.0f) {
      speed *= SHROOM_POWERUP_SPEED_MULTIPLIER;
    }
    velocity = ShroomVec2Scale(player->input_direction, speed * delta_time);

    player->position = ShroomVec2Add(player->position, velocity);
    player->radius = ShroomMassToRadius(player->mass);
    player->position.x =
        ShroomClamp(player->position.x, player->radius, world->width - player->radius);
    player->position.y =
        ShroomClamp(player->position.y, player->radius, world->height - player->radius);

    /* Apply and decay split impulse. */
    if (ShroomVec2LengthSqr(player->split_velocity) > 1.0f) {
      float decay;
      player->position =
          ShroomVec2Add(player->position, ShroomVec2Scale(player->split_velocity, delta_time));
      player->position.x =
          ShroomClamp(player->position.x, player->radius, world->width - player->radius);
      player->position.y =
          ShroomClamp(player->position.y, player->radius, world->height - player->radius);
      decay = 1.0f - (SHROOM_SPLIT_IMPULSE_DECAY * delta_time);
      if (decay < 0.0f) {
        decay = 0.0f;
      }
      player->split_velocity = ShroomVec2Scale(player->split_velocity, decay);
      if (ShroomVec2LengthSqr(player->split_velocity) < 1.0f) {
        player->split_velocity = (ShroomVec2){0};
      }
    }

    /* Tick down merge timer. */
    if (player->merge_timer > 0.0f) {
      player->merge_timer -= delta_time;
      if (player->merge_timer < 0.0f) {
        player->merge_timer = 0.0f;
      }
    }
    if (player->spawn_protection_timer > 0.0f) {
      player->spawn_protection_timer -= delta_time;
      if (player->spawn_protection_timer < 0.0f) {
        player->spawn_protection_timer = 0.0f;
      }
    }
    if (player->eject_cooldown_timer > 0.0f) {
      player->eject_cooldown_timer -= delta_time;
      if (player->eject_cooldown_timer < 0.0f) {
        player->eject_cooldown_timer = 0.0f;
      }
    }
    if (player->bot_tactical_cooldown_timer > 0.0f) {
      player->bot_tactical_cooldown_timer -= delta_time;
      if (player->bot_tactical_cooldown_timer < 0.0f) {
        player->bot_tactical_cooldown_timer = 0.0f;
      }
    }
  }

  ShroomUpdatePlayerEffects(world, delta_time);
  ShroomCollectSpores(world, spore_grid, delta_time);
  ShroomCollectPowerups(world);
  ShroomResolveConsumes(world);

  const bool objective_won = ShroomUpdateKingOfHill(world, delta_time);
  ShroomUpdateColonyRoundStats(world, delta_time);
  ShroomApplyMassRules(world, delta_time, step_end_time_ms);
  ShroomApplyForcedSplits(world);
  ShroomResolveMerges(world);
  ShroomUpdatePowerups(world, delta_time);

  world->match_time_remaining -= delta_time;
  if (objective_won || (world->match_time_remaining <= 0.0f)) {
    world->match_time_remaining = 0.0f;
    ShroomComputeMatchPodium(world);
    world->match_phase = SHROOM_MATCH_PHASE_RESULTS;
    world->match_results_time_remaining = SHROOM_MATCH_RESULTS_SECONDS;
  }

  world->tick += 1;
}
