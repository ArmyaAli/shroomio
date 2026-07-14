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
  SHROOM_POWERUP_MAGNET = SHROOM_POWERUP_TYPE_MAGNET,
  SHROOM_POWERUP_DECAY_IMMUNE = SHROOM_POWERUP_TYPE_DECAY_IMMUNE,
} ShroomPowerupType;

typedef enum ShroomMatchPhase {
  SHROOM_MATCH_PHASE_RUNNING = 0,
  SHROOM_MATCH_PHASE_RESULTS = 1,
  SHROOM_MATCH_PHASE_RESET = 2,
} ShroomMatchPhase;

typedef enum ShroomBotRiskProfile {
  SHROOM_BOT_PROFILE_CONSERVATIVE = 0,
  SHROOM_BOT_PROFILE_AGGRESSIVE = 1,
  SHROOM_BOT_PROFILE_OBJECTIVE = 2,
  SHROOM_BOT_PROFILE_COUNT
} ShroomBotRiskProfile;

typedef struct ShroomRoundStats {
  float colony_mass;
  float peak_mass;
  float survival_seconds;
  uint32_t kills;
  uint32_t spores_collected;
  uint32_t powerups_collected;
  float center_zone_seconds;
  float mid_zone_seconds;
  float outer_zone_seconds;
  uint32_t splits_used;
  uint32_t ejects_used;
} ShroomRoundStats;

typedef struct ShroomRoundStatsSlot {
  ShroomPlayerId player_id;
  ShroomRoundStats stats;
} ShroomRoundStatsSlot;

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
  float magnet_powerup_timer;
  float decay_immune_powerup_timer;
  float eject_cooldown_timer;
  float bot_tactical_cooldown_timer;
  uint8_t piece_index;     /* 0 = primary; 1-3 = split fragment */
  uint8_t team_id;         /* 0 = no team (FFA), 1-8 = team number */
  uint8_t life_generation; /* 1-255; advances whenever the primary starts a new life */
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
  ShroomMatchPhase match_phase;
  float match_duration_seconds;
  float match_time_remaining;
  float match_results_time_remaining;
  ShroomPlayerId podium_player_ids[SHROOM_MATCH_PODIUM_COUNT];
  float podium_masses[SHROOM_MATCH_PODIUM_COUNT];
  ShroomGameMode game_mode;
  float objective_target_score;
  ShroomPlayerId objective_controller_id; /* 0 = empty or contested */
  bool objective_contested;
  ShroomPlayerId objective_player_ids[SHROOM_MAX_PARTICIPANTS];
  float objective_scores[SHROOM_MAX_PARTICIPANTS];
  uint8_t team_count;                  /* number of teams in team-based modes */
  float team_masses[SHROOM_MAX_TEAMS]; /* combined mass per team */
  size_t player_count;
  size_t spore_count;
  size_t powerup_count;
  ShroomPlayerState players[SHROOM_MAX_PLAYER_ENTITIES];
  ShroomSporeState spores[SHROOM_MAX_SPORES];
  ShroomPowerupState powerups[SHROOM_MAX_POWERUPS];
  ShroomRoundStatsSlot round_stats[SHROOM_MAX_PARTICIPANTS];
} ShroomWorldState;

#endif
