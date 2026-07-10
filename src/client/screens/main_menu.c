#include "game.h"
#include "layout.h"
#include "screen.h"
#include "screen_background.h"

#include "imgui_wrapper.h"
#include "raylib.h"

typedef enum MainMenuAction {
  MAIN_MENU_ACTION_NONE = 0,
  MAIN_MENU_ACTION_PLAY_ONLINE,
  MAIN_MENU_ACTION_CUSTOM_SERVER,
  MAIN_MENU_ACTION_OFFLINE_PRACTICE,
  MAIN_MENU_ACTION_WATCH_GAME,
  MAIN_MENU_ACTION_GAME_MODES,
  MAIN_MENU_ACTION_SETTINGS,
  MAIN_MENU_ACTION_HELP,
  MAIN_MENU_ACTION_CREDITS,
  MAIN_MENU_ACTION_EXIT,
} MainMenuAction;

static bool MainMenuAnimationsEnabled(const Game* game) {
  return (game == NULL) || game->settings.menu_animations_enabled;
}

#ifdef TEST_MODE
/* Exposed for the imgui test harness so the #334 regression (hard-coded
 * return true;) is caught directly instead of relying on visual inspection. */
bool ShroomTestMainMenuAnimationsEnabled(const Game* game) {
  return MainMenuAnimationsEnabled(game);
}
#endif

static bool MainMenuInit(ShroomScreenManager* manager) {
  (void)manager;
  ShroomScreenResetFungalBackground();
  return true;
}

static void MainMenuUpdate(ShroomScreenManager* manager, float delta_time) {
  const Game* game = manager != NULL ? (const Game*)manager->user_data : NULL;
  ShroomScreenUpdateFungalBackground(delta_time, MainMenuAnimationsEnabled(game));
}

static void MainMenuDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  const bool animate = MainMenuAnimationsEnabled(game);
  const float panel_width = 340.0f;
  const float panel_height = 514.0f;
  MainMenuAction action = MAIN_MENU_ACTION_NONE;

  ShroomScreenDrawFungalBackground(animate);

  if (!ShroomLayoutBeginCenteredPanel("Main Menu", panel_width, panel_height, 0.85f,
                                      SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                                          SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                                          SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_Text("SHROOMIO");
  ShroomImGui_TextWrapped(
      "Grow by collecting spores, out-position bigger threats, and take over the arena.");
  ShroomImGui_Spacing();

  if (ShroomLayoutButtonFullWidth("Play Online", 38.0f)) {
    action = MAIN_MENU_ACTION_PLAY_ONLINE;
  }
  if (ShroomLayoutButtonFullWidth("Custom Server", 38.0f)) {
    action = MAIN_MENU_ACTION_CUSTOM_SERVER;
  }
  if (ShroomLayoutButtonFullWidth("Offline Practice", 38.0f)) {
    action = MAIN_MENU_ACTION_OFFLINE_PRACTICE;
  }
  if (ShroomLayoutButtonFullWidth("Watch Game", 38.0f)) {
    action = MAIN_MENU_ACTION_WATCH_GAME;
  }
  if (ShroomLayoutButtonFullWidth("Game Modes", 38.0f)) {
    action = MAIN_MENU_ACTION_GAME_MODES;
  }
  if (ShroomLayoutButtonFullWidth("Settings", 38.0f)) {
    action = MAIN_MENU_ACTION_SETTINGS;
  }
  if (ShroomLayoutButtonFullWidth("Help", 38.0f)) {
    action = MAIN_MENU_ACTION_HELP;
  }
  if (ShroomLayoutButtonFullWidth("Credits", 38.0f)) {
    action = MAIN_MENU_ACTION_CREDITS;
  }
  if (ShroomLayoutButtonFullWidth("Exit", 38.0f)) {
    action = MAIN_MENU_ACTION_EXIT;
  }

  ShroomImGui_End();

  if (action == MAIN_MENU_ACTION_NONE) {
    return;
  }

  GamePlayUiClickSound(game);
  switch (action) {
  case MAIN_MENU_ACTION_PLAY_ONLINE:
    TraceLog(LOG_INFO, "MENU: Play Online selected host=%s port=%u",
             game != NULL ? game->selected_server_host : "<null>",
             game != NULL ? (unsigned int)game->selected_server_port : 0u);
    if (game != NULL) {
      TraceLog(LOG_INFO, "MENU: Play Online ClientNetInit begin");
      ClientNetInit(&game->net, game->selected_server_host, game->selected_server_port);
      TraceLog(LOG_INFO, "MENU: Play Online ClientNetInit end status=%d host=%p peer=%p",
               (int)game->net.status, (void*)game->net.host, (void*)game->net.peer);
      game->auto_join_lobby = true;
    }
    TraceLog(LOG_INFO, "MENU: Play Online transition to lobby");
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_LOBBY);
    TraceLog(LOG_INFO, "MENU: Play Online transition complete");
    break;
  case MAIN_MENU_ACTION_CUSTOM_SERVER:
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
    break;
  case MAIN_MENU_ACTION_OFFLINE_PRACTICE:
    if (game != NULL) {
      game->selected_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
      game->start_in_spectator_mode = false;
    }
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
    break;
  case MAIN_MENU_ACTION_WATCH_GAME:
    if (game != NULL) {
      game->selected_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
      game->start_in_spectator_mode = true;
    }
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
    break;
  case MAIN_MENU_ACTION_GAME_MODES:
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME_MODE_SELECT);
    break;
  case MAIN_MENU_ACTION_SETTINGS:
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SETTINGS);
    break;
  case MAIN_MENU_ACTION_HELP:
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_HELP);
    break;
  case MAIN_MENU_ACTION_CREDITS:
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_CREDITS);
    break;
  case MAIN_MENU_ACTION_EXIT:
    ShroomScreenManagerRequestExit(manager);
    break;
  case MAIN_MENU_ACTION_NONE:
  default:
    break;
  }
}

static void MainMenuHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerRequestExit(manager);
  }
}

static void MainMenuCleanup(ShroomScreenManager* manager) { (void)manager; }

void ShroomScreenRegisterMainMenu(ShroomScreenManager* manager) {
  ShroomScreen* screen;

  if (manager == NULL) {
    return;
  }

  screen = &manager->screens[SHROOM_SCREEN_MAIN_MENU];
  screen->type = SHROOM_SCREEN_MAIN_MENU;
  screen->name = "Main Menu";
  screen->init = MainMenuInit;
  screen->update = MainMenuUpdate;
  screen->draw = MainMenuDraw;
  screen->handle_input = MainMenuHandleInput;
  screen->cleanup = MainMenuCleanup;
}
