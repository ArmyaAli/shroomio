#ifndef SHROOM_CLIENT_GAME_H
#define SHROOM_CLIENT_GAME_H

#include <stdint.h>

#include "raylib.h"

#include "client_settings.h"
#include "net.h"
#include "shared/world.h"

#define SHROOM_CLIENT_PENDING_INPUT_CAPACITY 128u

typedef enum GameSessionMode {
  SHROOM_SESSION_MODE_QUICK_PLAY = 0,
  SHROOM_SESSION_MODE_OFFLINE_PRACTICE,
} GameSessionMode;

typedef struct ShroomPendingInput {
  uint32_t sequence;
  ShroomVec2 direction;
} ShroomPendingInput;

typedef struct Game {
  Camera2D camera;
  ClientSettings settings;
  ClientNetState net;
  ShroomWorldState world;
  ShroomPlayerState* local_player;
  GameSessionMode selected_mode;
  GameSessionMode active_mode;
  ShroomZone current_zone;
  ShroomPendingInput pending_inputs[SHROOM_CLIENT_PENDING_INPUT_CAPACITY];
  uint32_t pending_input_count;
  uint32_t tracked_input_sequence;
  ShroomVec2 render_positions[SHROOM_MAX_PLAYERS];
  ShroomVec2 previous_local_position;
  bool render_position_initialized[SHROOM_MAX_PLAYERS];
  bool leaderboard_overlay_open;
  bool menu_overlay_open;
  bool diagnostics_overlay_open;
  bool leave_confirmation_open;
  bool inspect_overlay_open;
  bool return_to_menu_requested;
  bool chat_open;
  bool chat_minimized;
  bool chat_focus_input;
  float chat_inactive_timer;
  char chat_input_buf[SHROOM_CHAT_MAX_MESSAGE_LENGTH + 1u];
  bool chat_scroll_to_bottom;
  float inspect_overlay_progress;
  float inspect_prompt_timer;
  float previous_local_mass;
  float zone_callout_timer;
  float respawn_banner_timer;
  int screen_width;
  int screen_height;
  int selected_inspect_index;
  int inspect_target_count;
  uint32_t selected_inspect_player_id;
  char selected_server_host[64];
  uint16_t selected_server_port;
} Game;

void GameInit(Game* game, int screen_width, int screen_height, GameSessionMode mode);
void GameUpdate(Game* game, float delta_time);
void GameDraw(Game* game);
void GameShutdown(Game* game);

#endif
