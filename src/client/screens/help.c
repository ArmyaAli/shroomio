#include "game.h"
#include "screen.h"
#include "screen_background.h"

#include "imgui_wrapper.h"
#include "raylib.h"

static bool HelpInit(ShroomScreenManager* manager) {
  (void)manager;
  return true;
}

static void HelpDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  const int screen_width = GetScreenWidth();
  const int screen_height = GetScreenHeight();

  ShroomScreenDrawFungalBackground((game == NULL) || game->settings.menu_animations_enabled);

  ShroomImGui_SetNextWindowPos((float)screen_width * 0.14f, (float)screen_height * 0.12f,
                               SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize((float)screen_width * 0.72f, (float)screen_height * 0.72f,
                                SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.88f);
  if (!ShroomImGui_Begin("How To Play", NULL,
                         SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                             SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_TextWrapped("Stay alive by growing faster than the lobby. Movement follows the "
                          "mouse, with keyboard nudging available for sharper cuts and escapes.");
  ShroomImGui_Separator();
  ShroomImGui_Text("Core Rules");
  ShroomImGui_TextWrapped("Collect spores to gain mass and become harder to consume.");
  ShroomImGui_TextWrapped("Consume players who are at least 15% smaller than you.");
  ShroomImGui_TextWrapped("Avoid heavier players and watch the off-screen indicators for danger.");
  ShroomImGui_TextWrapped("The center zone is the riskiest but has the biggest swing potential.");
  ShroomImGui_Separator();
  ShroomImGui_Text("Overlay Shortcuts");
  {
    char shortcut_buf[192];
    /* Reflect the current keybindings so the help text matches the player's
     * saved config rather than hardcoded defaults. */
    snprintf(
        shortcut_buf, sizeof(shortcut_buf),
        "Tab opens the leaderboard, %s opens the match menu, %s toggles diagnostics, %s cycles "
        "HUD density. %s opens chat (or Enter). %s is reserved for push-to-talk.",
        ClientSettingsKeyLabel(game->settings.key_pause_menu), ClientSettingsKeyLabel(KEY_F3),
        ClientSettingsKeyLabel(game->settings.key_hud_toggle),
        ClientSettingsKeyLabel(game->settings.key_chat_open),
        ClientSettingsKeyLabel(game->settings.key_push_to_talk));
    ShroomImGui_TextWrapped(shortcut_buf);
  }
  ShroomImGui_Spacing();

  if (ShroomImGui_Button("Back", 140.0f, 36.0f)) {
    GamePlayUiClickSound(game);
    ShroomScreenManagerGoBack(manager);
  }

  ShroomImGui_End();
}

static void HelpHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerGoBack(manager);
  }
}

void ShroomScreenRegisterHelp(ShroomScreenManager* manager) {
  ShroomScreen* screen;

  if (manager == NULL) {
    return;
  }

  screen = &manager->screens[SHROOM_SCREEN_HELP];
  screen->type = SHROOM_SCREEN_HELP;
  screen->name = "Help";
  screen->init = HelpInit;
  screen->draw = HelpDraw;
  screen->handle_input = HelpHandleInput;
}
