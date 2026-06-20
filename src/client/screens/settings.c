#include "game.h"
#include "screen.h"
#include "screen_background.h"

#include "imgui_wrapper.h"
#include "raylib.h"

typedef struct SettingsScreenState {
  ClientSettings pending;
  bool dirty;
  bool save_succeeded;
} SettingsScreenState;

static SettingsScreenState g_settings_screen;

static const char* const kRegionItems[] = {
    "Auto",
    "Europe",
    "North America",
};

static const char* const kPaletteItems[] = {
    "Classic",
    "High Contrast",
};

static void ApplySettings(Game* game) {
  game->settings = g_settings_screen.pending;
  SetMasterVolume((float)game->settings.master_volume_percent / 100.0f);
}

static bool SettingsInit(ShroomScreenManager* manager) {
  const Game* game = manager != NULL ? (const Game*)manager->user_data : NULL;

  if (game == NULL) {
    return false;
  }

  g_settings_screen.pending = game->settings;
  g_settings_screen.dirty = false;
  g_settings_screen.save_succeeded = false;
  return true;
}

static void SettingsDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  bool changed = false;
  const int screen_width = GetScreenWidth();
  const int screen_height = GetScreenHeight();

  if (game == NULL) {
    return;
  }

  ShroomScreenDrawFungalBackground(game->settings.menu_animations_enabled);

  ShroomImGui_SetNextWindowPos((float)screen_width * 0.18f, (float)screen_height * 0.1f,
                               SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize((float)screen_width * 0.64f, (float)screen_height * 0.8f,
                                SHROOM_IMGUI_COND_ALWAYS);
  if (!ShroomImGui_Begin("Settings", NULL,
                         SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                             SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_Text("Interface");
  changed |= ShroomImGui_SliderInt("UI Scale", &g_settings_screen.pending.ui_scale_percent, 80, 160,
                                   "%d%%");
  changed |= ShroomImGui_Combo("Preferred Region",
                               &g_settings_screen.pending.preferred_region_index, kRegionItems, 3);
  changed |= ShroomImGui_Combo("Palette", (int*)&g_settings_screen.pending.palette_preset,
                               kPaletteItems, 2);

  ShroomImGui_Separator();
  ShroomImGui_Text("Audio");
  changed |= ShroomImGui_SliderInt(
      "Master Volume", &g_settings_screen.pending.master_volume_percent, 0, 100, "%d%%");
  changed |= ShroomImGui_SliderInt("Music Volume", &g_settings_screen.pending.music_volume_percent,
                                   0, 100, "%d%%");
  changed |= ShroomImGui_SliderInt(
      "Effects Volume", &g_settings_screen.pending.effects_volume_percent, 0, 100, "%d%%");

  ShroomImGui_Separator();
  ShroomImGui_Text("Gameplay");
  changed |= ShroomImGui_Checkbox("Invert Mouse", &g_settings_screen.pending.invert_mouse);
  changed |= ShroomImGui_Checkbox("Show Diagnostics On Launch",
                                  &g_settings_screen.pending.diagnostics_enabled);
  changed |= ShroomImGui_Checkbox("Show Ping In HUD", &g_settings_screen.pending.show_ping_ms);
  changed |= ShroomImGui_Checkbox("Animated Menu Backgrounds",
                                  &g_settings_screen.pending.menu_animations_enabled);

  if (changed) {
    ClientSettingsValidate(&g_settings_screen.pending);
    g_settings_screen.dirty = true;
    g_settings_screen.save_succeeded = false;
    ApplySettings(game);
  }

  ShroomImGui_Spacing();
  if (g_settings_screen.save_succeeded) {
    ShroomImGui_Text("Saved to client_settings.cfg");
  } else if (g_settings_screen.dirty) {
    ShroomImGui_Text("Unsaved changes");
  }

  if (ShroomImGui_Button("Save", 140.0f, 36.0f)) {
    ClientSettingsValidate(&g_settings_screen.pending);
    ApplySettings(game);
    g_settings_screen.save_succeeded = ClientSettingsSave(&game->settings);
    g_settings_screen.dirty = !g_settings_screen.save_succeeded;
  }
  ShroomImGui_SameLine();
  if (ShroomImGui_Button("Back", 140.0f, 36.0f)) {
    ShroomScreenManagerGoBack(manager);
  }

  ShroomImGui_End();
}

static void SettingsHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerGoBack(manager);
  }
}

void ShroomScreenRegisterSettings(ShroomScreenManager* manager) {
  ShroomScreen* screen;

  if (manager == NULL) {
    return;
  }

  screen = &manager->screens[SHROOM_SCREEN_SETTINGS];
  screen->type = SHROOM_SCREEN_SETTINGS;
  screen->name = "Settings";
  screen->init = SettingsInit;
  screen->draw = SettingsDraw;
  screen->handle_input = SettingsHandleInput;
}
