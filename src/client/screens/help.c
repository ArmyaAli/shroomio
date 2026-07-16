#include "game.h"
#include "game_mode_availability.h"
#include "layout.h"
#include "layout_metrics.h"
#include "screen.h"
#include "screen_background.h"

#include "imgui_wrapper.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>

static int g_help_active_tab;

#ifdef TEST_MODE
#define SHROOM_HELP_MAX_RENDERED_ITEMS 12
static char g_help_rendered_items[SHROOM_HELP_MAX_RENDERED_ITEMS][160];
static int g_help_rendered_item_count;
static char g_help_rendered_heading[32];
static bool g_help_final_row_visible;
static bool g_help_demo_controls_visible;
#endif

static bool HelpInit(ShroomScreenManager* manager) {
  (void)manager;
  g_help_active_tab = 0;
  return true;
}

static void DrawTabButton(const char* label, int index, float width, bool same_line) {
  ShroomImGuiColor tab_color;
  const bool is_active = (g_help_active_tab == index);

  if (is_active) {
    tab_color = (ShroomImGuiColor){0.28f, 0.56f, 0.88f, 1.0f};
    ShroomImGui_PushStyleColor(SHROOM_IMGUI_COL_BUTTON, tab_color.r, tab_color.g, tab_color.b,
                               tab_color.a);
  }
  if (ShroomImGui_Button(label, width, ShroomLayoutMetric(32.0f))) {
    g_help_active_tab = index;
  }
  if (is_active) {
    ShroomImGui_PopStyleColor();
  }
  if (same_line) {
    ShroomImGui_SameLine();
  }
}

static bool CanDemoColonyConsume(float attacker_mass, float target_mass) {
  return attacker_mass >= target_mass * SHROOM_CONSUME_MASS_ADVANTAGE;
}

static void DrawSectionCard(const char* title, ShroomImGuiColor header_color, const char** items,
                            int item_count) {
  const float card_width = ShroomImGui_GetContentRegionAvailWidth();
  const float wrap_width = card_width - ShroomLayoutMetric(40.0f);
  float text_height = 0.0f;

  for (int index = 0; index < item_count; ++index) {
    text_height += ShroomImGui_CalcWrappedTextHeight(items[index], wrap_width);
  }
  const float card_height =
      ShroomLayoutWrappedCardHeight(text_height, item_count, ShroomLayoutGetScale());

  ShroomImGui_PushStyleColor(SHROOM_IMGUI_COL_CHILD_BG, 0.14f, 0.12f, 0.10f, 0.85f);
  ShroomImGui_BeginChild(title, 0.0f, card_height, true);

  ShroomImGui_TextColored(header_color, title);
  ShroomImGui_Separator();

  ShroomImGui_Spacing();
#ifdef TEST_MODE
  snprintf(g_help_rendered_heading, sizeof(g_help_rendered_heading), "%s", title);
  g_help_rendered_item_count =
      item_count < SHROOM_HELP_MAX_RENDERED_ITEMS ? item_count : SHROOM_HELP_MAX_RENDERED_ITEMS;
#endif
  for (int i = 0; i < item_count; ++i) {
    ShroomImGui_TextWrapped(items[i]);
#ifdef TEST_MODE
    if (i < SHROOM_HELP_MAX_RENDERED_ITEMS) {
      snprintf(g_help_rendered_items[i], sizeof(g_help_rendered_items[i]), "%s", items[i]);
    }
    if (i == item_count - 1) {
      g_help_final_row_visible = ShroomImGui_IsLastItemVisible();
    }
#endif
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
  char rule_buf[192];
  ShroomLayoutResponsiveRow button_row;
#ifdef TEST_MODE
  bool demo_buttons_visible = true;
#endif

  ShroomImGui_Text("Growth Rules Demo");
  snprintf(rule_buf, sizeof(rule_buf),
           "Click a colony to compare mass. Outside the center, a colony must be at least %.0f%% "
           "heavier to consume another.",
           (SHROOM_CONSUME_MASS_ADVANTAGE - 1.0f) * 100.0f);
  ShroomImGui_TextWrapped(rule_buf);
  button_row =
      ShroomLayoutResponsiveRowMetrics(ShroomImGui_GetContentRegionAvailWidth(),
                                       ShroomLayoutMetric(112.0f), 3, ShroomLayoutMetric(10.0f));
  if (ShroomImGui_Button("Sprout 86", button_row.item_width, ShroomLayoutMetric(32.0f))) {
    selected_colony = 0;
  }
#ifdef TEST_MODE
  demo_buttons_visible = demo_buttons_visible && ShroomImGui_IsLastItemVisible();
#endif
  if (button_row.columns > 1) {
    ShroomImGui_SameLine();
  }
  if (ShroomImGui_Button("Cluster 112", button_row.item_width, ShroomLayoutMetric(32.0f))) {
    selected_colony = 1;
  }
#ifdef TEST_MODE
  demo_buttons_visible = demo_buttons_visible && ShroomImGui_IsLastItemVisible();
#endif
  if (button_row.columns == 3) {
    ShroomImGui_SameLine();
  }
  if (ShroomImGui_Button("Bloom 148", button_row.item_width, ShroomLayoutMetric(32.0f))) {
    selected_colony = 2;
  }
#ifdef TEST_MODE
  demo_buttons_visible = demo_buttons_visible && ShroomImGui_IsLastItemVisible();
  g_help_demo_controls_visible = demo_buttons_visible;
#endif

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
    ShroomImGui_TextColoredWrapped(
        CanDemoColonyConsume(colony_masses[selected], colony_masses[index]) ? success_color
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
             "Safe for now: no demo colony is %.0f%% heavier than %s.",
             (SHROOM_CONSUME_MASS_ADVANTAGE - 1.0f) * 100.0f, colony_names[selected]);
  }
  ShroomImGui_TextDisabledWrapped(detail_buf);
}

static void HelpDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  const char* tab_labels[] = {"Controls", "Gameplay", "Zones", "Modes"};
  ShroomLayoutResponsiveRow tab_row;
  float content_height;

  if (game == NULL) {
    return;
  }
#ifdef TEST_MODE
  g_help_final_row_visible = false;
  g_help_demo_controls_visible = false;
#endif
  ShroomScreenDrawFungalBackground(game->settings.menu_animations_enabled);

  if (!ShroomLayoutBeginCenteredPanel("How To Play", 900.0f, 650.0f, 0.90f,
                                      SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                                          SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                                          SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_TextWrapped("Stay alive by growing faster than the lobby. Movement follows the "
                          "mouse, with keyboard nudging available for sharper cuts and escapes.");

  ShroomImGui_Separator();
  tab_row =
      ShroomLayoutResponsiveRowMetrics(ShroomImGui_GetContentRegionAvailWidth(),
                                       ShroomLayoutMetric(104.0f), 4, ShroomLayoutMetric(10.0f));
  for (int index = 0; index < 4; ++index) {
    DrawTabButton(tab_labels[index], index, tab_row.item_width,
                  ((index + 1) % tab_row.columns) != 0 && index < 3);
  }
  ShroomImGui_Spacing();

  content_height =
      ShroomLayoutReservedContentHeight(ShroomImGui_GetContentRegionAvailHeight(),
                                        ShroomLayoutMetric(36.0f), ShroomLayoutMetric(8.0f));
  ShroomImGui_BeginChild("HelpContent", 0.0f, content_height, false);

  if (g_help_active_tab == 0) {
    char split_line[96];
    char eject_line[96];
    char chat_line[96];
    char hud_line[96];
    char pause_line[96];
    const char* items[9];

    snprintf(split_line, sizeof(split_line), "[Space] Hold %.1fs to split (mass %.0f+)",
             SHROOM_SPLIT_HOLD_SECONDS, SHROOM_SPLIT_MIN_MASS);
    snprintf(eject_line, sizeof(eject_line), "[E / RMB] Eject mass (mass %.0f+)",
             SHROOM_EJECT_MIN_MASS);
    snprintf(chat_line, sizeof(chat_line), "[%s / Enter] Open online chat",
             ClientSettingsKeyLabel(game->settings.key_chat_open));
    snprintf(hud_line, sizeof(hud_line), "[%s] Cycle HUD density",
             ClientSettingsKeyLabel(game->settings.key_hud_toggle));
    snprintf(pause_line, sizeof(pause_line), "[%s] Pause / match menu",
             ClientSettingsKeyLabel(game->settings.key_pause_menu));
    items[0] = "[Mouse] Move toward the cursor";
    items[1] = "[WASD / Arrows] Nudge movement direction";
    items[2] = split_line;
    items[3] = eject_line;
    items[4] = chat_line;
    items[5] = "[Tab] Leaderboard / cycle split pieces";
    items[6] = hud_line;
    items[7] = pause_line;
    items[8] = "[F3] Toggle diagnostics";
    DrawSectionCard("Controls", (ShroomImGuiColor){0.28f, 0.56f, 0.88f, 1.0f}, items, 9);
    ShroomImGui_Spacing();
    ShroomImGui_Text("Aim and movement preview");
    ShroomImGui_DrawControlPreview(ShroomImGui_GetContentRegionAvailWidth(),
                                   ShroomLayoutMetric(170.0f), (float)GetTime());
  }

  if (g_help_active_tab == 1) {
    char spore_line[96];
    char consume_line[112];
    char split_line[112];
    char eject_line[112];
    char idle_line[96];
    const char* items[7];

    snprintf(spore_line, sizeof(spore_line), "Each spore grants %d mass.", SHROOM_SPORE_VALUE);
    snprintf(consume_line, sizeof(consume_line),
             "Be %.0f%% heavier (%.0f%% in center); gain %.0f%% of consumed mass.",
             (SHROOM_CONSUME_MASS_ADVANTAGE - 1.0f) * 100.0f,
             (SHROOM_CENTER_CONSUME_ADVANTAGE - 1.0f) * 100.0f,
             SHROOM_CONSUME_MASS_GAIN_FACTOR * 100.0f);
    snprintf(split_line, sizeof(split_line),
             "Split at mass %.0f+: lose %.0f%% mass, up to %d pieces, merge after %.0fs.",
             SHROOM_SPLIT_MIN_MASS, SHROOM_SPLIT_MASS_LOSS_FRACTION * 100.0f,
             SHROOM_MAX_SPLIT_PIECES, SHROOM_SPLIT_MERGE_SECONDS);
    snprintf(eject_line, sizeof(eject_line),
             "Eject at mass %.0f+: launch %.0f mass plus a %.0f%% cost.", SHROOM_EJECT_MIN_MASS,
             SHROOM_EJECT_MASS_VALUE, SHROOM_EJECT_COST_FRACTION * 100.0f);
    snprintf(idle_line, sizeof(idle_line), "Keep moving: idle mass loss starts after %.0fs.",
             SHROOM_IDLE_PENALTY_GRACE_SECONDS);
    items[0] = spore_line;
    items[1] = consume_line;
    items[2] = split_line;
    items[3] = eject_line;
    char protection_line[112];
    snprintf(protection_line, sizeof(protection_line),
             "Fresh players have %.1fs protection; new split pieces have %.1fs.",
             SHROOM_PLAYER_SPAWN_PROTECTION_SECONDS, SHROOM_SPLIT_PROTECTION_SECONDS);
    items[4] = protection_line;
    items[5] = "Powerups grant speed, shield, magnet, or decay immunity.";
    items[6] = idle_line;
    DrawSectionCard("Growth Rules", (ShroomImGuiColor){0.52f, 0.80f, 0.44f, 1.0f}, items, 7);
    ShroomImGui_Spacing();
    DrawGrowthRulesDemo();
  }

  if (g_help_active_tab == 2) {
    char center_line[128];
    char mid_line[128];
    char outer_line[128];
    const char* items[] = {center_line, mid_line, outer_line};

    snprintf(center_line, sizeof(center_line),
             "Center: consume at %.0f%% advantage; decay starts at mass %.0f (%.0f%%/s excess).",
             (SHROOM_CENTER_CONSUME_ADVANTAGE - 1.0f) * 100.0f, SHROOM_DEFAULT_PLAYER_MASS * 2.0f,
             SHROOM_DECAY_RATE_CENTER_PER_SECOND * 100.0f);
    snprintf(mid_line, sizeof(mid_line),
             "Mid: consume at %.0f%% advantage; decay starts at mass %.0f (%.0f%%/s excess).",
             (SHROOM_CONSUME_MASS_ADVANTAGE - 1.0f) * 100.0f, SHROOM_DECAY_MASS_THRESHOLD,
             SHROOM_DECAY_RATE_MID_PER_SECOND * 100.0f);
    snprintf(outer_line, sizeof(outer_line),
             "Outer: consume at %.0f%% advantage; no reachable zone decay threshold.",
             (SHROOM_CONSUME_MASS_ADVANTAGE - 1.0f) * 100.0f);
    DrawSectionCard("Zones", (ShroomImGuiColor){0.92f, 0.70f, 0.28f, 1.0f}, items, 3);
    ShroomImGui_Spacing();
    ShroomImGui_Text("Zone risk preview");
    ShroomImGui_DrawZonePreview(ShroomImGui_GetContentRegionAvailWidth(),
                                ShroomLayoutMetric(170.0f), (float)GetTime());
  }

  if (g_help_active_tab == 3) {
    char mode_lines[SHROOM_GAME_MODE_COUNT][160];
    const char* items[SHROOM_GAME_MODE_COUNT];
    size_t mode_count = 0u;
    const ShroomGameModeCapability* modes = ShroomGameModeCapabilities(&mode_count);

    for (size_t index = 0; index < mode_count; ++index) {
      snprintf(mode_lines[index], sizeof(mode_lines[index]), "%s - %s", modes[index].label,
               modes[index].available ? "Available" : "Unavailable");
      items[index] = mode_lines[index];
    }
    DrawSectionCard("Game Modes", (ShroomImGuiColor){0.72f, 0.48f, 0.84f, 1.0f}, items,
                    (int)mode_count);
  }

  ShroomImGui_EndChild();

  ShroomImGui_Spacing();
  if (ShroomImGui_Button("Back", ShroomLayoutMetric(140.0f), ShroomLayoutMetric(36.0f))) {
    GamePlayUiClickSound(game);
    ShroomScreenManagerGoBack(manager);
  }

  ShroomImGui_End();
}

#ifdef TEST_MODE
bool ShroomTestHelpRenderedTextContains(const char* text) {
  if (text == NULL) {
    return false;
  }
  for (int index = 0; index < g_help_rendered_item_count; ++index) {
    if (strstr(g_help_rendered_items[index], text) != NULL) {
      return true;
    }
  }
  return false;
}

const char* ShroomTestHelpRenderedHeading(void) { return g_help_rendered_heading; }

bool ShroomTestHelpFinalRowVisible(void) { return g_help_final_row_visible; }

bool ShroomTestHelpDemoControlsVisible(void) { return g_help_demo_controls_visible; }
#endif

static void HelpHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerGoBack(manager);
  }
  if (IsKeyPressed(KEY_RIGHT) && (g_help_active_tab < 3)) {
    g_help_active_tab += 1;
  }
  if (IsKeyPressed(KEY_LEFT) && (g_help_active_tab > 0)) {
    g_help_active_tab -= 1;
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
