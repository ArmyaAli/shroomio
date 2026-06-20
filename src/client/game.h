#ifndef SHROOM_CLIENT_GAME_H
#define SHROOM_CLIENT_GAME_H

#include <stdint.h>

#include "raylib.h"

#include "client_settings.h"
#include "net.h"
#include "shared/world.h"

#define SHROOM_CLIENT_PENDING_INPUT_CAPACITY 128u
#define SHROOM_CLIENT_PARTICLE_CAPACITY 384u

typedef enum GameSessionMode {
  SHROOM_SESSION_MODE_QUICK_PLAY = 0,
  SHROOM_SESSION_MODE_OFFLINE_PRACTICE,
  SHROOM_SESSION_MODE_LOBBY_PLAY, /* connected via lobby browser, net already up */
} GameSessionMode;

typedef struct LeaderboardEntry {
  size_t index;
  float mass;
} LeaderboardEntry;

typedef struct ShroomPendingInput {
  uint32_t sequence;
  ShroomVec2 direction;
} ShroomPendingInput;

typedef struct GameplayParticle {
  Vector2 position;
  Vector2 velocity;
  Color color;
  float age;
  float lifetime;
  float radius;
  bool active;
} GameplayParticle;

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
  uint32_t particle_cursor;
  ShroomVec2 render_positions[SHROOM_MAX_PLAYERS];
  ShroomVec2 previous_local_position;
  GameplayParticle particles[SHROOM_CLIENT_PARTICLE_CAPACITY];
  ShroomVec2 previous_spore_positions[SHROOM_MAX_SPORES];
  ShroomEntityId previous_spore_entity_ids[SHROOM_MAX_SPORES];
  ShroomVec2 previous_player_positions[SHROOM_MAX_PLAYERS];
  ShroomEntityId previous_player_entity_ids[SHROOM_MAX_PLAYERS];
  ShroomPlayerId previous_player_ids[SHROOM_MAX_PLAYERS];
  float previous_player_masses[SHROOM_MAX_PLAYERS];
  bool previous_player_alive[SHROOM_MAX_PLAYERS];
  ShroomVec2 previous_powerup_positions[SHROOM_MAX_POWERUPS];
  ShroomEntityId previous_powerup_entity_ids[SHROOM_MAX_POWERUPS];
  bool previous_powerup_active[SHROOM_MAX_POWERUPS];
  bool render_position_initialized[SHROOM_MAX_PLAYERS];
  bool particle_baseline_ready;
  bool leaderboard_overlay_open;
  bool menu_overlay_open;
  bool diagnostics_overlay_open;
  bool leave_confirmation_open;
  bool inspect_overlay_open;
  bool return_to_menu_requested;
  float camera_zoom_target;
  bool split_requested;
  float split_hold_timer;           /* seconds Space held; fires at SHROOM_SPLIT_HOLD_SECONDS */
  bool piece_focus_changed;         /* set when Tab cycles pieces */
  bool local_has_split;             /* true after local player uses their one split per life */
  uint32_t focused_piece_entity_id; /* entity_id of the piece being controlled; 0 = primary */
  int local_piece_count;            /* how many alive pieces the local player has */
  bool auto_join_lobby;             /* set by Play Online — join best lobby on list receipt */
  bool chat_open;
  bool chat_minimized;
  bool chat_focus_input;
  float chat_inactive_timer;
  char chat_input_buf[SHROOM_CHAT_MAX_MESSAGE_LENGTH + 1u];
  bool chat_scroll_to_bottom;
  float ambient_particle_timer;
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
  /* Session statistics for results screen */
  float session_start_time;
  float peak_mass;
  float final_mass;
  int final_rank;
  bool show_results;
} Game;

void GameInit(Game* game, int screen_width, int screen_height, GameSessionMode mode);
void GameUpdate(Game* game, float delta_time);
void GameDraw(Game* game);
void GameShutdown(Game* game);

/* Leaderboard and ranking functions */
void BuildLeaderboard(const Game* game, LeaderboardEntry* entries, size_t* entry_count);
int GetLocalPlayerRank(const Game* game, const LeaderboardEntry* leaderboard,
                       size_t leaderboard_count);

#endif
