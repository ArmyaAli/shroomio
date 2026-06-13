#ifndef SHROOM_CONFIG_H
#define SHROOM_CONFIG_H

#define SHROOM_MAX_PLAYERS 128
#define SHROOM_MAX_SPORES 4096
#define SHROOM_MAX_NAME_LENGTH 32u

#define SHROOM_WORLD_WIDTH 6000.0f
#define SHROOM_WORLD_HEIGHT 6000.0f

#define SHROOM_ZONE_CENTER_RADIUS 900.0f
#define SHROOM_ZONE_MID_RADIUS 2000.0f

#define SHROOM_DEFAULT_PLAYER_MASS 96.0f
#define SHROOM_BOT_COUNT 18

// Keep small colonies nimble while letting large colonies become catchable.
#define SHROOM_MIN_PLAYER_SPEED 132.0f
#define SHROOM_MAX_PLAYER_SPEED 252.0f
#define SHROOM_PLAYER_SPEED_MASS_SCALE 0.42f

// More spores with slightly lower value smooths early growth and recovery pacing.
#define SHROOM_SPORE_TARGET_COUNT 1100
#define SHROOM_SPORE_VALUE 7

// Tighten consume rules and reduce snowballing from single catches.
#define SHROOM_CONSUME_MASS_ADVANTAGE 1.18f
#define SHROOM_CONSUME_MASS_GAIN_FACTOR 0.58f

// Give fresh spawns more breathing room before re-entering contested space.
#define SHROOM_SPAWN_SAFE_DISTANCE 440.0f

#define SHROOM_SERVER_TICK_RATE 30.0f

#endif
