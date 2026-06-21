#include "sim.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static float ShroomClamp(float value, float min_value, float max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }

  return value;
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

  for (index = 0; index < world->player_count; ++index) {
    const ShroomPlayerState* other = &world->players[index];
    const float safety_radius = other->radius + SHROOM_SPAWN_SAFE_DISTANCE;

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
  const float padding = 120.0f;
  size_t attempt;

  for (attempt = 0; attempt < 32; ++attempt) {
    const ShroomVec2 candidate = {
        ShroomRandomFloat(world, padding, world->width - padding),
        ShroomRandomFloat(world, padding, world->height - padding),
    };
    const ShroomZone zone = ShroomGetZoneAtPosition(world, candidate);

    if (prefer_outer && zone != SHROOM_ZONE_OUTER) {
      continue;
    }
    if (ShroomIsSafeSpawn(world, candidate)) {
      return candidate;
    }
  }

  return (ShroomVec2){ShroomRandomFloat(world, padding, world->width - padding),
                      ShroomRandomFloat(world, padding, world->height - padding)};
}

static void ShroomRespawnPlayer(ShroomWorldState* world, ShroomPlayerState* player) {
  player->position = ShroomRandomSpawnPosition(world, true);
  player->input_direction = (ShroomVec2){0};
  player->mass = SHROOM_DEFAULT_PLAYER_MASS;
  player->radius = ShroomMassToRadius(player->mass);
  player->decay_spore_accumulator = 0.0f;
  player->alive = true;
  player->ai_controlled = false;
  player->has_split = false;
  player->split_velocity = (ShroomVec2){0};
  player->merge_timer = 0.0f;
  player->spawn_protection_timer = 0.0f;
  player->speed_powerup_timer = 0.0f;
  player->shield_powerup_timer = 0.0f;
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
  powerup->type = (slot_index % 2u) == 0u ? SHROOM_POWERUP_SPEED : SHROOM_POWERUP_SHIELD;
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

static void ShroomUpdateBotInput(ShroomWorldState* world, ShroomPlayerState* bot) {
  size_t index;
  const ShroomPlayerState* best_threat = 0;
  const ShroomPlayerState* best_prey = 0;
  const ShroomSporeState* best_spore = 0;
  float best_threat_score = 0.0f;
  float best_prey_score = 0.0f;
  float best_spore_score = 0.0f;
  float best_threat_distance = 0.0f;
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
      }
    }
  }

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

  if ((best_prey != 0) && (best_prey_score > 1.2f)) {
    bot->input_direction = ShroomNormalizeOrZero(ShroomVec2Sub(best_prey->position, bot->position));
    return;
  }

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

#define SHROOM_SPORE_GRID_CELL_SIZE 200.0f
#define SHROOM_SPORE_GRID_COLS (((int)(SHROOM_WORLD_WIDTH / SHROOM_SPORE_GRID_CELL_SIZE)) + 1)
#define SHROOM_SPORE_GRID_ROWS (((int)(SHROOM_WORLD_HEIGHT / SHROOM_SPORE_GRID_CELL_SIZE)) + 1)
#define SHROOM_SPORE_GRID_CELLS (SHROOM_SPORE_GRID_COLS * SHROOM_SPORE_GRID_ROWS)
#define SHROOM_SPORE_GRID_MAX_PER_CELL 64

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

static void ShroomCollectSpores(ShroomWorldState* world) {
  size_t player_index;
  ShroomSporeGridCell grid[SHROOM_SPORE_GRID_CELLS];

  ShroomBuildSporeGrid(world, grid);

  for (player_index = 0; player_index < world->player_count; ++player_index) {
    ShroomPlayerState* player = &world->players[player_index];
    const float collection_radius = player->radius + 6.0f;
    int min_col, max_col, min_row, max_row, col, row;

    if (!player->alive) {
      continue;
    }

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
            player->mass =
                ShroomClamp(player->mass + (float)spore->value, 0.0f, SHROOM_MAX_PLAYER_MASS);
            ShroomSpawnOrResetSpore(world, spore);
          }
        }
      }
    }
  }
}

static bool ShroomCanConsume(const ShroomWorldState* world, const ShroomPlayerState* attacker,
                             const ShroomPlayerState* target) {
  float overlap_radius = attacker->radius * 0.88f;
  const float required_advantage = ShroomGetConsumeMassAdvantageAtPosition(world, target->position);
  const float boundary_margin = 100.0f;
  bool near_boundary = false;

  if (!attacker->alive || !target->alive) {
    return false;
  }
  /* Pieces of the same player cannot consume each other. */
  if (attacker->player_id == target->player_id) {
    return false;
  }
  if (target->shield_powerup_timer > 0.0f) {
    return false;
  }
  if (target->spawn_protection_timer > 0.0f) {
    return false;
  }
  if (attacker->mass < (target->mass * required_advantage)) {
    return false;
  }

  /* Check if either player is near a boundary */
  if (attacker->position.x < boundary_margin ||
      attacker->position.x > (world->width - boundary_margin) ||
      attacker->position.y < boundary_margin ||
      attacker->position.y > (world->height - boundary_margin) ||
      target->position.x < boundary_margin ||
      target->position.x > (world->width - boundary_margin) ||
      target->position.y < boundary_margin ||
      target->position.y > (world->height - boundary_margin)) {
    near_boundary = true;
  }

  /* Use more generous overlap check near boundaries */
  if (near_boundary) {
    overlap_radius = attacker->radius * 1.2f + target->radius * 0.5f;
  }

  return ShroomDistanceSqr(attacker->position, target->position) <=
         (overlap_radius * overlap_radius);
}

static void ShroomResolveConsumes(ShroomWorldState* world) {
  size_t attacker_index;
  bool consumed[SHROOM_MAX_PLAYERS] = {0};
  size_t consumed_by[SHROOM_MAX_PLAYERS] = {0};

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

      if (ShroomCanConsume(world, attacker, target)) {
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

      winner->mass = ShroomClamp(winner->mass + (victim->mass * SHROOM_CONSUME_MASS_GAIN_FACTOR),
                                 0.0f, SHROOM_MAX_PLAYER_MASS);
      victim->alive = false;
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

static void ShroomApplyMassRules(ShroomWorldState* world, float delta_time) {
  size_t index;

  for (index = 0; index < world->player_count; ++index) {
    ShroomPlayerState* player = &world->players[index];

    if (!player->alive) {
      continue;
    }

    player->mass = ShroomClamp(player->mass, 0.0f, SHROOM_MAX_PLAYER_MASS);
    {
      const ShroomZone zone = ShroomGetZoneAtPosition(world, player->position);
      const float decay_threshold = zone == SHROOM_ZONE_CENTER ? (SHROOM_DEFAULT_PLAYER_MASS * 2.0f)
                                    : zone == SHROOM_ZONE_MID  ? SHROOM_DECAY_MASS_THRESHOLD
                                                               : SHROOM_MAX_PLAYER_MASS;

      if (player->mass <= decay_threshold) {
        player->radius = ShroomMassToRadius(player->mass);
        continue;
      }

      const float excess_mass = player->mass - decay_threshold;
      const float decay_mass = excess_mass * SHROOM_DECAY_RATE_PER_SECOND * delta_time;
      const float eject_radius = player->radius + 12.0f;
      const float angle = ShroomRandomFloat(world, 0.0f, 6.2831853f);
      const ShroomVec2 spore_position = {
          ShroomClamp(player->position.x + (cosf(angle) * eject_radius), 60.0f,
                      world->width - 60.0f),
          ShroomClamp(player->position.y + (sinf(angle) * eject_radius), 60.0f,
                      world->height - 60.0f),
      };
      uint16_t spore_value;

      player->mass = ShroomClamp(player->mass - decay_mass, 0.0f, SHROOM_MAX_PLAYER_MASS);
      player->decay_spore_accumulator += decay_mass;
      spore_value = (uint16_t)player->decay_spore_accumulator;
      if (spore_value > 0u) {
        ShroomSpawnDecaySpore(world, spore_position, spore_value);
        player->decay_spore_accumulator -= (float)spore_value;
      }
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

ShroomPlayerState* ShroomWorldSpawnPlayer(ShroomWorldState* world, ShroomPlayerId player_id,
                                          bool is_bot) {
  ShroomPlayerState* player;
  size_t index;

  for (index = 0; index < world->player_count; ++index) {
    if (!world->players[index].alive) {
      player = &world->players[index];
      goto initialize_player;
    }
  }

  if (world->player_count >= SHROOM_MAX_PLAYERS) {
    return 0;
  }

  player = &world->players[world->player_count++];

initialize_player:
  *player = (ShroomPlayerState){
      .player_id = player_id,
      .entity_id = world->next_entity_id++,
      .position = ShroomRandomSpawnPosition(world, true),
      .mass = SHROOM_DEFAULT_PLAYER_MASS,
      .radius = ShroomMassToRadius(SHROOM_DEFAULT_PLAYER_MASS),
      .alive = true,
      .is_bot = is_bot,
  };

  return player;
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

bool ShroomWorldSplitPlayer(ShroomWorldState* world, ShroomPlayerState* player) {
  ShroomPlayerState* new_piece;
  float half_mass;
  ShroomVec2 launch_dir;
  int piece_count;
  size_t i;

  if ((world == NULL) || (player == NULL) || !player->alive) {
    return false;
  }
  if (player->mass < SHROOM_SPLIT_MIN_MASS) {
    return false;
  }
  /* One voluntary split per life for real players. */
  if (!player->is_bot && player->has_split) {
    return false;
  }
  piece_count = ShroomCountPlayerPieces(world, player->player_id);
  if (piece_count >= SHROOM_MAX_SPLIT_PIECES) {
    return false;
  }

  /* Find a free slot (zeroed-out merged/fragment slot) or extend the array. */
  new_piece = NULL;
  for (i = 0; i < world->player_count; ++i) {
    if (!world->players[i].alive && (world->players[i].player_id == 0)) {
      new_piece = &world->players[i];
      break;
    }
  }
  if (new_piece == NULL) {
    if (world->player_count >= SHROOM_MAX_PLAYERS) {
      return false;
    }
    new_piece = &world->players[world->player_count++];
  }

  half_mass = player->mass / 2.0f;
  launch_dir = ShroomNormalizeOrZero(player->input_direction);
  if (ShroomVec2LengthSqr(launch_dir) < 0.0001f) {
    launch_dir = (ShroomVec2){1.0f, 0.0f};
  }

  *new_piece = (ShroomPlayerState){
      .player_id = player->player_id,
      .entity_id = world->next_entity_id++,
      .position = player->position,
      .input_direction = player->input_direction,
      .mass = half_mass,
      .radius = ShroomMassToRadius(half_mass),
      .alive = true,
      .is_bot = player->is_bot,
      .ai_controlled = true,
      .merge_timer = SHROOM_SPLIT_MERGE_SECONDS,
      .spawn_protection_timer = SHROOM_SPLIT_PROTECTION_SECONDS,
      .piece_index = (uint8_t)piece_count,
      .split_velocity = ShroomVec2Scale(launch_dir, SHROOM_SPLIT_IMPULSE_SPEED),
  };
  snprintf(new_piece->name, sizeof(new_piece->name), "%s", player->name);

  player->mass = half_mass;
  player->radius = ShroomMassToRadius(half_mass);
  player->has_split = true;
  if (player->merge_timer <= 0.0f) {
    player->merge_timer = SHROOM_SPLIT_MERGE_SECONDS;
  }
  player->spawn_protection_timer = SHROOM_SPLIT_PROTECTION_SECONDS;

  return true;
}

static bool ShroomCanMergePieces(const ShroomPlayerState* primary, const ShroomPlayerState* piece) {
  const float merge_radius = primary->radius + piece->radius;

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
    if (piece->merge_timer > 0.0f) {
      continue;
    }
    primary = ShroomFindPrimaryPiece(world, piece->player_id);
    if ((primary == NULL) || !primary->alive || (primary->merge_timer > 0.0f)) {
      continue;
    }
    if (!ShroomCanMergePieces(primary, piece)) {
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
    if (!player->alive || !player->is_bot) {
      continue;
    }
    if (player->mass < SHROOM_SPLIT_MASS_THRESHOLD) {
      continue;
    }
    if (ShroomCountPlayerPieces(world, player->player_id) >= SHROOM_MAX_SPLIT_PIECES) {
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

void ShroomWorldStep(ShroomWorldState* world, float delta_time) {
  size_t index;

  for (index = 0; index < world->player_count; ++index) {
    ShroomPlayerState* player = &world->players[index];

    if ((player->is_bot || player->ai_controlled) && player->alive) {
      ShroomUpdateBotInput(world, player);
    }
  }

  for (index = 0; index < world->player_count; ++index) {
    ShroomPlayerState* player = &world->players[index];
    float speed = ShroomMassToSpeed(player->mass);
    ShroomVec2 velocity;

    if (!player->alive) {
      continue;
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
  }

  ShroomUpdatePlayerEffects(world, delta_time);
  ShroomCollectSpores(world);
  ShroomCollectPowerups(world);
  ShroomResolveConsumes(world);
  ShroomApplyMassRules(world, delta_time);
  ShroomApplyForcedSplits(world);
  ShroomResolveMerges(world);
  ShroomUpdatePowerups(world, delta_time);

  world->tick += 1;
}
