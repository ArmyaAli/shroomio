#ifndef SHROOM_CLIENT_GAME_H
#define SHROOM_CLIENT_GAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "raylib.h"

#include "client_settings.h"
#include "net.h"
#include "prediction.h"
#include "quick_match.h"
#include "shared/world.h"

#define SHROOM_CLIENT_PENDING_INPUT_CAPACITY 128u
#define SHROOM_CLIENT_PARTICLE_CAPACITY 384u
#define SHROOM_CLIENT_NOTIFICATION_CAPACITY 6u
#define SHROOM_CLIENT_GAMEPLAY_EVENT_CAPACITY 64u
#define SHROOM_CLIENT_PROXIMITY_SPORE_DOT_BUDGET 96u

static inline bool ShroomClientShouldSampleIndexedItem(size_t index, size_t total_count,
                                                       size_t budget) {
  size_t stride;

  if ((budget == 0u) || (total_count == 0u)) {
    return false;
  }
  if (total_count <= budget) {
    return true;
  }

  stride = (total_count + budget - 1u) / budget;
  return (index % stride) == 0u;
}

typedef enum GameSessionMode {
  SHROOM_SESSION_MODE_QUICK_PLAY = 0,
  SHROOM_SESSION_MODE_OFFLINE_PRACTICE,
  SHROOM_SESSION_MODE_LOBBY_PLAY, /* connected via lobby browser, net already up */
} GameSessionMode;

typedef struct LeaderboardEntry {
  size_t index;
  ShroomPlayerId player_id;
  float mass;
  float objective_score;
} LeaderboardEntry;

typedef struct GameplayParticle {
  Vector2 position;
  Vector2 velocity;
  Color color;
  float age;
  float lifetime;
  float radius;
  bool active;
} GameplayParticle;

typedef struct CombatNotification {
  char title[96];
  char detail[128];
  Color color;
  float age;
  float duration;
  bool active;
} CombatNotification;

typedef struct KillFeedEntry {
  char text[128];
  uint64_t event_key;
  Color color;
  float age;
  float duration;
  bool active;
} KillFeedEntry;

typedef enum GameplayEventType {
  GAMEPLAY_EVENT_PARTICLE_BURST = 0,
  GAMEPLAY_EVENT_NOTIFICATION,
  GAMEPLAY_EVENT_SCREEN_FLASH,
  GAMEPLAY_EVENT_SFX,
  GAMEPLAY_EVENT_DEATH_CUTSCENE,
  GAMEPLAY_EVENT_ZONE_CALLOUT,
  GAMEPLAY_EVENT_RESPAWN_BANNER,
} GameplayEventType;

typedef struct GameplayEvent {
  GameplayEventType type;
  ShroomVec2 position;
  Color color;
  int count;
  float speed;
  float radius;
  float lifetime;
  float duration;
  float importance;
  float final_mass;
  float survival_time;
  int final_rank;
  int sfx;
  char title[96];
  char detail[128];
  char name[SHROOM_MAX_NAME_LENGTH];
} GameplayEvent;

typedef struct Game {
  Camera2D camera;
  ClientSettings settings;
  ClientNetState net;
  ShroomWorldState world;
  ShroomPlayerState* local_player;
  GameSessionMode selected_mode;
  GameSessionMode active_mode;
  ShroomGameMode selected_game_mode;
  ShroomZone current_zone;
  ShroomPendingInput pending_inputs[SHROOM_CLIENT_PENDING_INPUT_CAPACITY];
  uint32_t pending_input_count;
  uint32_t tracked_input_sequence;
  uint64_t last_applied_snapshot_tick;
  bool snapshot_applied;
  uint32_t particle_cursor;
  uint32_t notification_head;
  uint32_t notification_count;
  uint32_t kill_feed_head;
  uint32_t kill_feed_count;
  uint32_t gameplay_event_head;
  uint32_t gameplay_event_count;
  ShroomVec2 render_positions[SHROOM_MAX_PLAYER_ENTITIES];
  ShroomVec2 previous_local_position;
  GameplayParticle particles[SHROOM_CLIENT_PARTICLE_CAPACITY];
  CombatNotification notifications[SHROOM_CLIENT_NOTIFICATION_CAPACITY];
  KillFeedEntry kill_feed[8];
  GameplayEvent gameplay_events[SHROOM_CLIENT_GAMEPLAY_EVENT_CAPACITY];
  ShroomVec2 previous_spore_positions[SHROOM_MAX_SPORES];
  ShroomEntityId previous_spore_entity_ids[SHROOM_MAX_SPORES];
  ShroomVec2 previous_player_positions[SHROOM_MAX_PLAYER_ENTITIES];
  ShroomEntityId previous_player_entity_ids[SHROOM_MAX_PLAYER_ENTITIES];
  ShroomPlayerId previous_player_ids[SHROOM_MAX_PLAYER_ENTITIES];
  char previous_player_names[SHROOM_MAX_PLAYER_ENTITIES][SHROOM_MAX_NAME_LENGTH];
  float previous_player_masses[SHROOM_MAX_PLAYER_ENTITIES];
  bool previous_player_alive[SHROOM_MAX_PLAYER_ENTITIES];
  uint8_t previous_player_piece_indices[SHROOM_MAX_PLAYER_ENTITIES];
  uint8_t previous_player_life_generations[SHROOM_MAX_PLAYER_ENTITIES];
  ShroomVec2 previous_powerup_positions[SHROOM_MAX_POWERUPS];
  ShroomEntityId previous_powerup_entity_ids[SHROOM_MAX_POWERUPS];
  bool previous_powerup_active[SHROOM_MAX_POWERUPS];
  bool render_position_initialized[SHROOM_MAX_PLAYER_ENTITIES];
  bool particle_baseline_ready;
  bool leaderboard_overlay_open;
  bool menu_overlay_open;
  bool diagnostics_overlay_open;
  bool leave_confirmation_open;
  bool inspect_overlay_open;
  bool spectator_mode;
  bool spectator_follow_mode;
  bool start_in_spectator_mode;
  bool return_to_menu_requested;
  bool play_again_requested;
  bool resume_online_session_requested;
  float camera_zoom_target;
  bool split_requested;
  float split_hold_timer; /* seconds Space held; fires at SHROOM_SPLIT_HOLD_SECONDS */
  ShroomVec2
      split_aim_direction; /* normalized launch aim, cursor direction with movement fallback */
  ShroomVec2 split_aim_visual_direction; /* smoothed direction used only for arrow rendering */
  bool piece_focus_changed;              /* set when Tab cycles pieces */
  bool local_has_split;                  /* true after local player uses their one split per life */
  uint32_t focused_piece_entity_id;      /* entity_id of the piece being controlled; 0 = primary */
  int local_piece_count;                 /* how many alive pieces the local player has */
  bool auto_join_lobby; /* set by Play Online — join best lobby on list receipt */
  ShroomQuickMatchState quick_match;
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
  int previous_local_rank;
  float combat_feedback_cooldown;
  float screen_flash_timer;
  float death_cutscene_timer;
  float death_cutscene_duration;
  float death_cutscene_final_mass;
  float death_cutscene_survival_time;
  Color screen_flash_color;
  int death_cutscene_final_rank;
  char death_cutscene_killer_name[SHROOM_MAX_NAME_LENGTH];
  float zone_callout_timer;
  float respawn_banner_timer;
  float death_camera_hold_timer; /* holds camera at death position so death FX are visible */
  Vector2 death_camera_hold_pos; /* world position to hold the camera at during death */
  int screen_width;
  int screen_height;
  int selected_inspect_index;
  int inspect_target_count;
  uint32_t spectated_entity_id;
  uint32_t selected_inspect_player_id;
  char selected_server_host[64];
  uint16_t selected_server_port;
  /* Session statistics for results screen */
  float session_start_time;
  uint32_t session_duration_seconds;
  float peak_mass;
  float final_mass;
  int final_rank;
  uint32_t final_spores_collected;
  uint32_t final_kills;
  bool show_results;
  bool authoritative_round_resume_pending;
  bool eject_requested;
} Game;

void GameInit(Game* game, int screen_width, int screen_height, GameSessionMode mode);
void GameHandleResize(Game* game, int screen_width, int screen_height);
void GameUpdate(Game* game, float delta_time);
void GameDraw(Game* game);
void GameUpdateVoice(Game* game);
void GameSuspendForResults(Game* game);
void GameShutdown(Game* game);
void GamePlayUiClickSound(const Game* game);
void GamePlayUiErrorSound(const Game* game);
void GameEnterSpectatorMode(Game* game);
void GameExitSpectatorMode(Game* game);
void GameCycleSpectatorTarget(Game* game, int direction);

#ifdef TEST_MODE
void GameTestSetMovementInput(ShroomVec2 direction);
void GameTestSetPushToTalk(bool enabled, bool held);
const char* ShroomTestGetDiagnosticsRatesText(void);
const char* ShroomTestGetDiagnosticsBandwidthText(void);
const char* ShroomTestGetDiagnosticsTransportText(void);
const char* ShroomTestGetDiagnosticsCadenceText(void);
const char* ShroomTestGetDiagnosticsActionsText(void);
#endif

/* Leaderboard and ranking functions */
void BuildLeaderboard(const Game* game, LeaderboardEntry* entries, size_t* entry_count);
int GetLocalPlayerRank(const Game* game, const LeaderboardEntry* leaderboard,
                       size_t leaderboard_count);

#endif
