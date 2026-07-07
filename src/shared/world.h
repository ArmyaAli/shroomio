#ifndef SHROOM_WORLD_H
#define SHROOM_WORLD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "vec2.h"

typedef uint32_t ShroomEntityId;
typedef uint32_t ShroomPlayerId;

typedef enum ShroomZone {
  SHROOM_ZONE_OUTER = 0,
  SHROOM_ZONE_MID = 1,
  SHROOM_ZONE_CENTER = 2,
} ShroomZone;

typedef enum ShroomPowerupType {
  SHROOM_POWERUP_SPEED = SHROOM_POWERUP_TYPE_SPEED,
  SHROOM_POWERUP_SHIELD = SHROOM_POWERUP_TYPE_SHIELD,
} ShroomPowerupType;

typedef struct ShroomPlayerState {
  ShroomPlayerId player_id;
  ShroomEntityId entity_id;
  ShroomVec2 position;
  ShroomVec2 input_direction;
  float mass;
  float radius;
  float decay_spore_accumulator;
  uint64_t last_move_time_ms;
  char name[SHROOM_MAX_NAME_LENGTH];
  bool alive;
  bool is_bot;
  bool ai_controlled;        /* server: non-focused split piece runs bot AI */
  bool has_split;            /* one voluntary split per life; reset on respawn */
  ShroomVec2 split_velocity; /* impulse applied on split, decays to zero */
  float merge_timer;         /* seconds until this piece may merge back; 0 = ready */
  float spawn_protection_timer;
  float speed_powerup_timer;
  float shield_powerup_timer;
  uint8_t piece_index; /* 0 = primary; 1-3 = split fragment */
} ShroomPlayerState;

typedef struct ShroomSporeState {
  ShroomEntityId entity_id;
  ShroomVec2 position;
  uint16_t value;
  bool active;
} ShroomSporeState;

typedef struct ShroomPowerupState {
  ShroomEntityId entity_id;
  ShroomVec2 position;
  ShroomPowerupType type;
  float respawn_timer;
  bool active;
} ShroomPowerupState;

typedef struct ShroomWorldState {
  uint64_t tick;
  float width;
  float height;
  uint32_t random_seed;
  uint32_t random_state;
  ShroomEntityId next_entity_id;
  size_t player_count;
  size_t spore_count;
  size_t powerup_count;
  ShroomPlayerState players[SHROOM_MAX_PLAYERS];
  ShroomSporeState spores[SHROOM_MAX_SPORES];
  ShroomPowerupState powerups[SHROOM_MAX_POWERUPS];
} ShroomWorldState;

#endif
