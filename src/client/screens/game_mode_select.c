#include "game.h"
#include "game_mode_availability.h"
#include "layout.h"
#include "screen.h"
#include "screen_background.h"

#include "imgui_wrapper.h"
#include "raylib.h"

#include <stdio.h>

static bool GameModeSelectInit(ShroomScreenManager* manager) {
  (void)manager;
  ShroomScreenResetFungalBackground();
  return true;
}

static void GameModeSelectUpdate(ShroomScreenManager* manager, float delta_time) {
  const Game* game = manager != NULL ? (const Game*)manager->user_data : NULL;
  ShroomScreenUpdateFungalBackground(delta_time,
                                     game != NULL && game->settings.menu_animations_enabled);
}

static void GameModeSelectDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  const bool animate = game != NULL && game->settings.menu_animations_enabled;
  const float panel_width = 500.0f;
  const float panel_height = 620.0f;
  size_t capability_count;
  const ShroomGameModeCapability* capabilities = ShroomGameModeCapabilities(&capability_count);

  ShroomScreenDrawFungalBackground(animate);

  if (!ShroomLayoutBeginCenteredPanel("Select Game Mode", panel_width, panel_height, 0.85f,
                                      SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                                          SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                                          SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_TextWrapped("Choose a mode. More formats will unlock as server support lands.");
  ShroomImGui_Spacing();

  for (size_t index = 0; index < capability_count; ++index) {
    const ShroomGameModeCapability* capability = &capabilities[index];
    char button_label[96];

    snprintf(button_label, sizeof(button_label), "%s%s", capability->label,
             capability->available ? "" : " - Unavailable");
    ShroomImGui_BeginDisabled(!capability->available);
    if (ShroomLayoutButtonFullWidth(button_label, 32.0f)) {
      GamePlayUiClickSound(game);
      game->selected_game_mode = capability->mode;
      ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
    }
    ShroomImGui_EndDisabled();
    ShroomImGui_TextDisabled(capability->summary);
  }

  ShroomImGui_Spacing();
  if (ShroomLayoutButtonFullWidth("Back", 38.0f)) {
    GamePlayUiClickSound(game);
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_MAIN_MENU);
  }

  ShroomImGui_End();
}

static void GameModeSelectHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_MAIN_MENU);
  }
}

void ShroomScreenRegisterGameModeSelect(ShroomScreenManager* manager) {
  ShroomScreen* screen;

  if (manager == NULL) {
    return;
  }

  screen = &manager->screens[SHROOM_SCREEN_GAME_MODE_SELECT];
  screen->type = SHROOM_SCREEN_GAME_MODE_SELECT;
  screen->name = "Game Mode Select";
  screen->init = GameModeSelectInit;
  screen->update = GameModeSelectUpdate;
  screen->draw = GameModeSelectDraw;
  screen->handle_input = GameModeSelectHandleInput;
}
