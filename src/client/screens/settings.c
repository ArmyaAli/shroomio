#include <stddef.h>

#include "game.h"
#include "screen.h"

#include "raygui.h"
#include "raylib.h"

static void SaveSettings(Game* game) {
  if (game == NULL) {
    return;
  }

  ClientSettingsValidate(&game->settings);
  ClientSettingsSave(&game->settings);
  SetMasterVolume((float)game->settings.master_volume_percent / 100.0f);
}

static bool SettingsInit(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  if (game == NULL) {
    return false;
  }

  ClientSettingsValidate(&game->settings);
  return true;
}

static void DrawSettingsSection(int x, int y, int width, int height, const char* title) {
  DrawRectangle(x, y, width, height, Fade(BLACK, 0.24f));
  DrawRectangleLines(x, y, width, height, Fade(RAYWHITE, 0.14f));
  DrawText(title, x + 18, y + 12, 24, RAYWHITE);
}

static void SettingsDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  int preferred_region_index;
  int palette_index;
  int invert_mouse;
  int diagnostics_enabled;
  int show_ping_ms;
  bool settings_changed = false;

  if (game == NULL) {
    return;
  }

  preferred_region_index = game->settings.preferred_region_index;
  palette_index = (int)game->settings.palette_preset;
  invert_mouse = game->settings.invert_mouse ? 1 : 0;
  diagnostics_enabled = game->settings.diagnostics_enabled ? 1 : 0;
  show_ping_ms = game->settings.show_ping_ms ? 1 : 0;

  BeginDrawing();
  ClearBackground((Color){28, 30, 46, 255});

  DrawText("SETTINGS", GetScreenWidth() / 2 - 86, 34, 36, RAYWHITE);
  DrawText("Changes save automatically and are restored on the next launch.",
           GetScreenWidth() / 2 - 288, 78, 22, GRAY);

  DrawSettingsSection(62, 126, 560, 212, "Graphics / UI");
  DrawText("UI Scale", 84, 176, 20, LIGHTGRAY);
  if (GuiSliderBar((Rectangle){244, 178, 280, 20}, NULL,
                   TextFormat("%d%%", game->settings.ui_scale_percent),
                   (float*)&game->settings.ui_scale_percent, 80.0f, 140.0f)) {
    settings_changed = true;
  }
  DrawText("Palette Preset", 84, 222, 20, LIGHTGRAY);
  if (GuiDropdownBox((Rectangle){244, 218, 188, 32}, "Classic;High Contrast", &palette_index,
                     false)) {
    (void)0;
  }
  if (palette_index != (int)game->settings.palette_preset) {
    game->settings.palette_preset = (ClientPalettePreset)palette_index;
    settings_changed = true;
  }
  DrawText("Show Ping in HUD", 84, 270, 20, LIGHTGRAY);
  if (GuiCheckBox((Rectangle){244, 268, 24, 24}, NULL, &show_ping_ms)) {
    settings_changed = true;
  }
  game->settings.show_ping_ms = show_ping_ms != 0;

  DrawSettingsSection(658, 126, 560, 212, "Audio");
  DrawText("Master Volume", 680, 176, 20, LIGHTGRAY);
  if (GuiSliderBar((Rectangle){850, 178, 280, 20}, NULL,
                   TextFormat("%d%%", game->settings.master_volume_percent),
                   (float*)&game->settings.master_volume_percent, 0.0f, 100.0f)) {
    settings_changed = true;
  }
  DrawText("Music Volume", 680, 222, 20, LIGHTGRAY);
  if (GuiSliderBar((Rectangle){850, 224, 280, 20}, NULL,
                   TextFormat("%d%%", game->settings.music_volume_percent),
                   (float*)&game->settings.music_volume_percent, 0.0f, 100.0f)) {
    settings_changed = true;
  }
  DrawText("Effects Volume", 680, 268, 20, LIGHTGRAY);
  if (GuiSliderBar((Rectangle){850, 270, 280, 20}, NULL,
                   TextFormat("%d%%", game->settings.effects_volume_percent),
                   (float*)&game->settings.effects_volume_percent, 0.0f, 100.0f)) {
    settings_changed = true;
  }

  DrawSettingsSection(62, 370, 560, 212, "Controls");
  DrawText("Invert Mouse Aim", 84, 420, 20, LIGHTGRAY);
  if (GuiCheckBox((Rectangle){244, 418, 24, 24}, NULL, &invert_mouse)) {
    settings_changed = true;
  }
  game->settings.invert_mouse = invert_mouse != 0;
  DrawText("Keyboard Layout", 84, 468, 20, LIGHTGRAY);
  DrawText("WASD and Arrow Keys are both available in-game.", 244, 468, 20, GRAY);
  DrawText("More rebind options will land in the dedicated keybinding issue.", 84, 510, 18, GRAY);

  DrawSettingsSection(658, 370, 560, 212, "Network");
  DrawText("Preferred Region", 680, 420, 20, LIGHTGRAY);
  if (GuiDropdownBox((Rectangle){850, 416, 210, 32}, "Auto;Europe;North America",
                     &preferred_region_index, false)) {
    (void)0;
  }
  if (preferred_region_index != game->settings.preferred_region_index) {
    game->settings.preferred_region_index = preferred_region_index;
    settings_changed = true;
  }
  DrawText("Diagnostics Panel", 680, 468, 20, LIGHTGRAY);
  if (GuiCheckBox((Rectangle){850, 466, 24, 24}, NULL, &diagnostics_enabled)) {
    settings_changed = true;
  }
  game->settings.diagnostics_enabled = diagnostics_enabled != 0;
  DrawText(ClientSettingsPreferredRegionLabel(game->settings.preferred_region_index), 1080, 420, 18,
           GRAY);
  DrawText(ClientSettingsPaletteLabel(game->settings.palette_preset), 446, 224, 18, GRAY);

  if (settings_changed) {
    SaveSettings(game);
  }

  DrawText("Safe fallback behavior: missing or invalid settings revert to defaults.", 62, 620, 18,
           GRAY);
  if (GuiButton((Rectangle){GetScreenWidth() / 2 - 100, 650, 200, 44}, "BACK")) {
    ShroomScreenManagerGoBack(manager);
  }

  EndDrawing();
}

static void SettingsHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerGoBack(manager);
  }
}

void ShroomScreenRegisterSettings(ShroomScreenManager* manager) {
  if (manager == NULL) {
    return;
  }

  ShroomScreen* screen = &manager->screens[SHROOM_SCREEN_SETTINGS];
  screen->type = SHROOM_SCREEN_SETTINGS;
  screen->name = "Settings";
  screen->init = SettingsInit;
  screen->draw = SettingsDraw;
  screen->handle_input = SettingsHandleInput;
}
