#ifndef SHROOM_CONFIG_H
#define SHROOM_CONFIG_H

#define SHROOM_MAX_PLAYERS 256
#define SHROOM_MAX_SPORES 4096
#define SHROOM_MAX_NAME_LENGTH 32u

#define SHROOM_WORLD_WIDTH 6000.0f
#define SHROOM_WORLD_HEIGHT 6000.0f

#define SHROOM_ZONE_CENTER_RADIUS 900.0f
#define SHROOM_ZONE_MID_RADIUS 2000.0f

#define SHROOM_DEFAULT_PLAYER_MASS 96.0f
#define SHROOM_BOT_COUNT 18
#define SHROOM_BOT_FLOOR 4u
#define SHROOM_BOT_TARGET_TOTAL 16u
#define SHROOM_BOT_REAL_PLAYER_WEIGHT 2u
#define SHROOM_DECAY_MASS_THRESHOLD (SHROOM_DEFAULT_PLAYER_MASS * 6.0f)
#define SHROOM_DECAY_RATE_PER_SECOND 0.02f
#define SHROOM_MAX_PLAYER_MASS (SHROOM_DEFAULT_PLAYER_MASS * 20.0f)
#define SHROOM_SPEED_FLOOR_FACTOR 0.60f

// Keep small colonies nimble while letting large colonies become catchable.
#define SHROOM_MIN_PLAYER_SPEED 132.0f
#define SHROOM_MAX_PLAYER_SPEED 252.0f
#define SHROOM_PLAYER_SPEED_MASS_SCALE 0.42f

// More spores with slightly lower value smooths early growth and recovery pacing.
#define SHROOM_SPORE_TARGET_COUNT 1100
#define SHROOM_SPORE_VALUE 7

// Tighten consume rules and reduce snowballing from single catches.
#define SHROOM_CONSUME_MASS_ADVANTAGE 1.18f
#define SHROOM_CENTER_CONSUME_ADVANTAGE 1.08f
#define SHROOM_CONSUME_MASS_GAIN_FACTOR 0.58f

// Give fresh spawns more breathing room before re-entering contested space.
#define SHROOM_SPAWN_SAFE_DISTANCE 440.0f

/* Splitting — players must be at max mass; bots are force-split at the same cap */
#define SHROOM_SPLIT_MIN_MASS SHROOM_MAX_PLAYER_MASS
#define SHROOM_SPLIT_MASS_THRESHOLD SHROOM_MAX_PLAYER_MASS
#define SHROOM_SPLIT_MERGE_SECONDS 10.0f
#define SHROOM_MAX_SPLIT_PIECES 4
#define SHROOM_SPLIT_IMPULSE_SPEED 220.0f
#define SHROOM_SPLIT_IMPULSE_DECAY 2.5f
#define SHROOM_SPLIT_HOLD_SECONDS 0.6f

#define SHROOM_SERVER_TICK_RATE 30.0f

/* Lobby configuration */
#define SHROOM_MAX_LOBBIES 8u
#define SHROOM_LOBBY_DEFAULT_COUNT 4u
#define SHROOM_LOBBY_MAX_NAME_LENGTH 32u
#define SHROOM_LOBBY_DYNAMIC_EMPTY_TIMEOUT_S 120u

#endif
