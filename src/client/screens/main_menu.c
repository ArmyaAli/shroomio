#include "game.h"
#include "account_flow.h"
#include "layout.h"
#include "screen.h"
#include "screen_background.h"

#include "imgui_wrapper.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>

#include <openssl/crypto.h>

static char g_player_name_input[SHROOM_MAX_NAME_LENGTH];
static bool g_account_panel_open;
static bool g_account_register_mode;
static char g_account_identity[SHROOM_CLIENT_REST_EMAIL_MAX + 1u];
static char g_account_username[SHROOM_CLIENT_REST_USERNAME_MAX + 1u];
static char g_account_email[SHROOM_CLIENT_REST_EMAIL_MAX + 1u];
static char g_account_password[129];

typedef enum MainMenuAction {
  MAIN_MENU_ACTION_NONE = 0,
  MAIN_MENU_ACTION_PLAY_ONLINE,
  MAIN_MENU_ACTION_CUSTOM_SERVER,
  MAIN_MENU_ACTION_OFFLINE_PRACTICE,
  MAIN_MENU_ACTION_WATCH_GAME,
  MAIN_MENU_ACTION_GAME_MODES,
  MAIN_MENU_ACTION_ACCOUNT,
  MAIN_MENU_ACTION_SETTINGS,
  MAIN_MENU_ACTION_HELP,
  MAIN_MENU_ACTION_CREDITS,
  MAIN_MENU_ACTION_EXIT,
} MainMenuAction;

static void ClearAccountInputs(void) {
  memset(g_account_identity, 0, sizeof(g_account_identity));
  memset(g_account_username, 0, sizeof(g_account_username));
  memset(g_account_email, 0, sizeof(g_account_email));
  OPENSSL_cleanse(g_account_password, sizeof(g_account_password));
}

static void DrawAccountPanel(Game* game) {
  ShroomAccountFlow* flow = game != NULL ? game->account_flow : NULL;
  ShroomAccountFlowState state;
  bool can_submit;

  if (!g_account_panel_open) {
    return;
  }
  if (!ShroomLayoutBeginCenteredPanel("Account", 440.0f, 410.0f, 0.97f,
                                      SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                                          SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                                          SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }
  state = flow != NULL ? ShroomAccountFlowPoll(flow) : SHROOM_ACCOUNT_FLOW_FAILED;
  /* Completed workers publish REST state before this acquire-and-acknowledge boundary. */
  if ((flow != NULL) && (state == SHROOM_ACCOUNT_FLOW_SUCCEEDED)) {
    ShroomAccountFlowAcknowledge(flow);
    state = SHROOM_ACCOUNT_FLOW_IDLE;
  }

  if (state == SHROOM_ACCOUNT_FLOW_WORKING) {
    /* Do not read REST fields while the worker may be mutating them. */
    ShroomLayoutHeading("Account");
    ShroomImGui_Text("Contacting account service...");
  } else if ((flow != NULL) && flow->rest->authenticated) {
    ShroomLayoutHeading("Account Profile");
    if (flow->rest->profile.username[0] != '\0') {
      ShroomImGui_Text(flow->rest->profile.username);
      ShroomImGui_Text(flow->rest->profile.email);
    }
    if (state == SHROOM_ACCOUNT_FLOW_FAILED) {
      ShroomImGui_TextColored((ShroomImGuiColor){1.0f, 0.45f, 0.4f, 1.0f},
                              flow->rest->error_message[0] != '\0'
                                  ? flow->rest->error_message
                                  : "The account profile could not be loaded.");
      if (ShroomLayoutButtonFullWidth("Retry Profile", 38.0f)) {
        if (ShroomAccountFlowStartGetMe(flow)) {
          state = SHROOM_ACCOUNT_FLOW_WORKING;
        }
      }
    }
    ShroomImGui_Spacing();
    ShroomImGui_BeginDisabled(state == SHROOM_ACCOUNT_FLOW_WORKING);
    if (ShroomLayoutButtonFullWidth("Log Out", 38.0f)) {
      (void)ShroomAccountFlowStartLogout(flow);
    }
    ShroomImGui_EndDisabled();
  } else if (flow == NULL) {
    ShroomImGui_TextColored((ShroomImGuiColor){1.0f, 0.45f, 0.4f, 1.0f},
                            "Account service is unavailable.");
  } else {
    /* Authentication is opt-in, so keep the credential form contained in this modal. */
    ShroomLayoutHeading(g_account_register_mode ? "Create Account" : "Sign In");
    if (ShroomImGui_Button("Existing Account", 150.0f, 32.0f)) {
      g_account_register_mode = false;
      ShroomAccountFlowAcknowledge(flow);
    }
    ShroomImGui_SameLine();
    if (ShroomImGui_Button("New Account", 150.0f, 32.0f)) {
      g_account_register_mode = true;
      ShroomAccountFlowAcknowledge(flow);
    }
    ShroomImGui_Spacing();
    if (g_account_register_mode) {
      ShroomLayoutSetNextLabeledItemWidth("Username");
      ShroomImGui_InputText("Username", g_account_username, sizeof(g_account_username));
      ShroomLayoutSetNextLabeledItemWidth("Email");
      ShroomImGui_InputText("Email", g_account_email, sizeof(g_account_email));
    } else {
      ShroomLayoutSetNextLabeledItemWidth("Username or Email");
      ShroomImGui_InputText("Username or Email", g_account_identity, sizeof(g_account_identity));
    }
    ShroomLayoutSetNextLabeledItemWidth("Password");
    ShroomImGui_InputTextPassword("Password", g_account_password, sizeof(g_account_password));
    can_submit = strlen(g_account_password) >= 12u;
    if (g_account_register_mode) {
      can_submit = can_submit && (strlen(g_account_username) >= 3u) &&
                   (strchr(g_account_email, '@') != NULL);
    } else {
      can_submit = can_submit && (g_account_identity[0] != '\0');
    }
    ShroomImGui_BeginDisabled(!can_submit || (state == SHROOM_ACCOUNT_FLOW_WORKING));
    if (ShroomLayoutButtonFullWidth(g_account_register_mode ? "Create Account" : "Sign In",
                                    38.0f)) {
      if (g_account_register_mode) {
        (void)ShroomAccountFlowStartRegister(flow, g_account_username, g_account_email,
                                             g_account_password);
      } else {
        (void)ShroomAccountFlowStartLogin(flow, g_account_identity, g_account_password);
      }
      OPENSSL_cleanse(g_account_password, sizeof(g_account_password));
    }
    ShroomImGui_EndDisabled();
    if (state == SHROOM_ACCOUNT_FLOW_FAILED) {
      ShroomImGui_TextColored((ShroomImGuiColor){1.0f, 0.45f, 0.4f, 1.0f},
                              flow->rest->error_message[0] != '\0' ? flow->rest->error_message
                                                                   : "The account request failed.");
    }
  }
  ShroomImGui_Spacing();
  ShroomImGui_BeginDisabled(state == SHROOM_ACCOUNT_FLOW_WORKING);
  if (ShroomLayoutButtonFullWidth("Close Account", 34.0f)) {
    if (flow != NULL) {
      ShroomAccountFlowAcknowledge(flow);
    }
    ClearAccountInputs();
    g_account_panel_open = false;
  }
  ShroomImGui_EndDisabled();
  ShroomImGui_End();
}

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
  const Game* game = manager != NULL ? (const Game*)manager->user_data : NULL;
  ShroomScreenResetFungalBackground();
  g_account_panel_open = false;
  ClearAccountInputs();
  snprintf(g_player_name_input, sizeof(g_player_name_input), "%s",
           game != NULL ? game->settings.player_name : "");
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
  const float panel_height =
      (game != NULL) && game->settings.account_features_enabled ? 558.0f : 514.0f;
  MainMenuAction action = MAIN_MENU_ACTION_NONE;

  ShroomScreenDrawFungalBackground(animate);

  if ((game != NULL) && (game->settings.player_name[0] == '\0')) {
    char sanitized[SHROOM_MAX_NAME_LENGTH];
    ClientSettingsSanitizePlayerName(sanitized, g_player_name_input);
    if (!ShroomLayoutBeginCenteredPanel(
            "Player Identity", 360.0f, 180.0f, 0.9f,
            SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                SHROOM_IMGUI_WINDOW_NO_COLLAPSE | SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
      ShroomImGui_End();
      return;
    }
    ShroomLayoutHeading("Choose Player Name");
    ShroomLayoutSetNextLabeledItemWidth("Player Name");
    ShroomImGui_InputText("Player Name", g_player_name_input, sizeof(g_player_name_input));
    ShroomImGui_BeginDisabled(sanitized[0] == '\0');
    if (ShroomLayoutButtonFullWidth("Continue", 38.0f)) {
      snprintf(game->settings.player_name, sizeof(game->settings.player_name), "%s", sanitized);
      ClientSettingsSave(&game->settings);
    }
    ShroomImGui_EndDisabled();
    ShroomImGui_End();
    return;
  }

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
  if ((game != NULL) && game->settings.account_features_enabled &&
      ShroomLayoutButtonFullWidth("Account", 38.0f)) {
    action = MAIN_MENU_ACTION_ACCOUNT;
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
  DrawAccountPanel(game);

  if (action == MAIN_MENU_ACTION_NONE) {
    return;
  }

  GamePlayUiClickSound(game);
  switch (action) {
  case MAIN_MENU_ACTION_PLAY_ONLINE:
    if (game != NULL) {
      ClientNetShutdown(&game->net);
      game->auto_join_lobby = false;
      ShroomQuickMatchBegin(&game->quick_match);
    }
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
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
  case MAIN_MENU_ACTION_ACCOUNT:
    g_account_panel_open = true;
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
