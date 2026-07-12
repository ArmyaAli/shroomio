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

static void DrawSectionCard(const char* title, ShroomImGuiColor header_color, const char** items,
                            int item_count) {
  const float card_width = 520.0f;
  const float header_height = 30.0f;
  const float item_height = 22.0f;
  const float card_height = header_height + (float)item_count * item_height + 20.0f;

  ShroomImGui_PushStyleColor(SHROOM_IMGUI_COL_CHILD_BG, 0.14f, 0.12f, 0.10f, 0.85f);
  ShroomImGui_BeginChild(title, card_width, card_height, true);

  ShroomImGui_PushStyleColor(SHROOM_IMGUI_COL_BUTTON, header_color.r, header_color.g,
                             header_color.b, header_color.a);
  ShroomImGui_Button(title, card_width - 16.0f, header_height);
  ShroomImGui_PopStyleColor();

  ShroomImGui_Spacing();
  for (int i = 0; i < item_count; ++i) {
    ShroomImGui_Text(items[i]);
  }

  ShroomImGui_EndChild();
  ShroomImGui_PopStyleColor();
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
                          "at least 15%% heavier.");
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

  ShroomImGui_SetNextWindowPos((float)screen_width * 0.08f, (float)screen_height * 0.08f,
                               SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize((float)screen_width * 0.84f, (float)screen_height * 0.84f,
                                SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.90f);
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

  {
    const char* items[] = {
        "[LMB]     Move toward cursor",
        "[Space]   Split — divide mass to catch prey",
        "[Q]       Eject mass — shed mass to gain speed",
        "[W]       Boost — spend mass for burst speed",
        "[Esc]     Pause / menu overlay",
        "[Tab]     Leaderboard overlay",
        "[F3]      Diagnostics overlay",
        "[F4]      Cycle HUD density",
        "[Enter]   Open chat",
    };
    DrawSectionCard("Controls", (ShroomImGuiColor){0.28f, 0.56f, 0.88f, 1.0f}, items, 9);
  }

  ShroomImGui_SameLine();
  ShroomImGui_Spacing();
  ShroomImGui_SameLine();

  {
    const char* items[] = {
        "Collect spores to gain mass.",
        "Consume players 15%% smaller to absorb their mass.",
        "Splitting creates two pieces — both must survive to merge back.",
        "Eject mass to shed weight and accelerate.",
        "Powerups grant temporary speed, shield, magnet, or decay immunity.",
        "Keep moving — idle colonies decay over time.",
    };
    DrawSectionCard("Growth Rules", (ShroomImGuiColor){0.52f, 0.80f, 0.44f, 1.0f}, items, 6);
  }

  ShroomImGui_Spacing();
  DrawGrowthRulesDemo();
  ShroomImGui_Separator();

  {
    const char* items[] = {
        "Center zone — highest risk, lowest consume advantage (8%%). Decay starts at 2x mass.",
        "Mid zone   — balanced risk. Decay starts at 6x mass at 3%%/sec.",
        "Outer zone — safest. No decay. Higher consume advantage (18%%).",
    };
    DrawSectionCard("Zones", (ShroomImGuiColor){0.92f, 0.70f, 0.28f, 1.0f}, items, 3);
  }

  ShroomImGui_SameLine();
  ShroomImGui_Spacing();
  ShroomImGui_SameLine();

  {
    const char* items[] = {
        "Free-for-All   — every colony for itself.",
        "Teams          — 2v2, 3v3, or 4v4.",
        "Battle Royale  — shrinking zone, last colony standing.",
        "King of the Hill — control the center to score points.",
        "Mass Race      — first to reach target mass wins.",
    };
    DrawSectionCard("Game Modes", (ShroomImGuiColor){0.72f, 0.48f, 0.84f, 1.0f}, items, 5);
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
