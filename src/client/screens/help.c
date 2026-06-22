#include "game.h"
#include "screen.h"
#include "screen_background.h"

#include "imgui_wrapper.h"
#include "raylib.h"

#include <stdio.h>

static bool HelpInit(ShroomScreenManager* manager) {
  (void)manager;
  return true;
}

static bool CanDemoColonyConsume(float attacker_mass, float target_mass) {
  return attacker_mass >= target_mass * 1.15f;
}

static void DrawGrowthRulesDemo(void) {
  static int selected_colony = 1;
  const char* colony_names[3] = {"Sprout", "Cluster", "Bloom"};
  const float colony_masses[3] = {86.0f, 112.0f, 148.0f};
  const ShroomImGuiColor success_color = {0.52f, 0.90f, 0.48f, 1.0f};
  const ShroomImGuiColor muted_color = {0.72f, 0.76f, 0.70f, 1.0f};
  const int selected = selected_colony;
  int danger_index = -1;
  char detail_buf[192];

  ShroomImGui_Text("Growth Rules Demo");
  ShroomImGui_TextWrapped("Click a colony to compare mass. A colony can consume another when it is "
                          "at least 15% heavier.");
  if (ShroomImGui_Button("Sprout 86", 112.0f, 32.0f)) {
    selected_colony = 0;
  }
  ShroomImGui_SameLine();
  if (ShroomImGui_Button("Cluster 112", 120.0f, 32.0f)) {
    selected_colony = 1;
  }
  ShroomImGui_SameLine();
  if (ShroomImGui_Button("Bloom 148", 112.0f, 32.0f)) {
    selected_colony = 2;
  }

  snprintf(detail_buf, sizeof(detail_buf), "Selected: %s mass %.0f", colony_names[selected],
           colony_masses[selected]);
  ShroomImGui_TextWrapped(detail_buf);

  for (int index = 0; index < 3; ++index) {
    if (index == selected) {
      continue;
    }
    snprintf(
        detail_buf, sizeof(detail_buf), "%s vs %s: %s", colony_names[selected], colony_names[index],
        CanDemoColonyConsume(colony_masses[selected], colony_masses[index]) ? "can consume"
                                                                            : "cannot consume yet");
    ShroomImGui_TextColored(CanDemoColonyConsume(colony_masses[selected], colony_masses[index])
                                ? success_color
                                : muted_color,
                            detail_buf);
    if ((danger_index < 0) && CanDemoColonyConsume(colony_masses[index], colony_masses[selected])) {
      danger_index = index;
    }
  }

  if (danger_index >= 0) {
    snprintf(detail_buf, sizeof(detail_buf), "Danger: %s can consume %s now.",
             colony_names[danger_index], colony_names[selected]);
  } else {
    snprintf(detail_buf, sizeof(detail_buf),
             "Safe for now: no demo colony is 15%% heavier than %s.", colony_names[selected]);
  }
  ShroomImGui_TextDisabled(detail_buf);
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
  DrawGrowthRulesDemo();
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
