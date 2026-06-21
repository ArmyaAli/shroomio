#include "game.h"
#include "screen.h"
#include "screen_background.h"

#include "imgui_wrapper.h"
#include "raylib.h"

#include <stdio.h>

#define SHROOM_KEYBINDING_SLOT_COUNT 4

typedef struct SettingsScreenState {
  ClientSettings pending;
  bool dirty;
  bool save_succeeded;
  /* Rebind capture state. slot == -1 means idle; 0..3 captures next key
   * for that slot. The captured-key path uses a single-frame queue so
   * the press that opens the rebind UI does not also assign. */
  int rebind_slot;
  bool rebind_armed;
  int rebind_conflict_slot; /* -1 if no conflict, else the slot that owns the pressed key */
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

static const char* const kParticleItems[] = {
    "Off",
    "Low",
    "Medium",
    "High",
};

static const ShroomMushroomSpeciesEntry* FindSpeciesEntry(const ClientNetState* net,
                                                          ClientMushroomSpecies species) {
  if ((net == NULL) || !net->mushroom_species_catalog_received) {
    return NULL;
  }
  for (uint8_t index = 0; index < net->mushroom_species_count; ++index) {
    if (net->mushroom_species[index].species_id == (uint8_t)species) {
      return &net->mushroom_species[index];
    }
  }
  return NULL;
}

static void ApplySettings(Game* game) {
  game->settings = g_settings_screen.pending;
  SetMasterVolume((float)game->settings.master_volume_percent / 100.0f);
}

static int* SettingsSlotKeyPtr(ClientSettings* settings, int slot) {
  if (settings == NULL) {
    return NULL;
  }
  switch (slot) {
  case 0:
    return &settings->key_chat_open;
  case 1:
    return &settings->key_hud_toggle;
  case 2:
    return &settings->key_pause_menu;
  case 3:
    return &settings->key_push_to_talk;
  default:
    return NULL;
  }
}

static int SettingsFindConflictSlot(const ClientSettings* settings, int key, int skip_slot) {
  int slot;
  if (settings == NULL || key <= KEY_NULL) {
    return -1;
  }
  for (slot = 0; slot < SHROOM_KEYBINDING_SLOT_COUNT; ++slot) {
    if (slot == skip_slot) {
      continue;
    }
    if (*SettingsSlotKeyPtr((ClientSettings*)settings, slot) == key) {
      return slot;
    }
  }
  return -1;
}

static bool SettingsInit(ShroomScreenManager* manager) {
  const Game* game = manager != NULL ? (const Game*)manager->user_data : NULL;

  if (game == NULL) {
    return false;
  }

  g_settings_screen.pending = game->settings;
  g_settings_screen.dirty = false;
  g_settings_screen.save_succeeded = false;
  g_settings_screen.rebind_slot = -1;
  g_settings_screen.rebind_armed = false;
  g_settings_screen.rebind_conflict_slot = -1;
  return true;
}

static void SettingsDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  bool changed = false;
  const char* species_items[CLIENT_MUSHROOM_COUNT];
  char species_fallback_items[CLIENT_MUSHROOM_COUNT][24];
  const ShroomMushroomSpeciesEntry* selected_species;
  const int screen_width = GetScreenWidth();
  const int screen_height = GetScreenHeight();

  if (game == NULL) {
    return;
  }

  for (int index = 0; index < CLIENT_MUSHROOM_COUNT; ++index) {
    const ShroomMushroomSpeciesEntry* entry =
        FindSpeciesEntry(&game->net, (ClientMushroomSpecies)index);
    if ((entry != NULL) && (entry->name[0] != '\0')) {
      species_items[index] = entry->name;
    } else {
      snprintf(species_fallback_items[index], sizeof(species_fallback_items[index]), "Species %d",
               index + 1);
      species_items[index] = species_fallback_items[index];
    }
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
  changed |= ShroomImGui_Combo(
      "Particle Quality", (int*)&g_settings_screen.pending.particle_quality, kParticleItems, 4);
  changed |= ShroomImGui_Checkbox("Animated Menu Backgrounds",
                                  &g_settings_screen.pending.menu_animations_enabled);
  changed |=
      ShroomImGui_Checkbox("Death Cutscene", &g_settings_screen.pending.death_cutscene_enabled);
  changed |=
      ShroomImGui_Combo("Mushroom Species", (int*)&g_settings_screen.pending.mushroom_species,
                        species_items, CLIENT_MUSHROOM_COUNT);
  selected_species = FindSpeciesEntry(&game->net, g_settings_screen.pending.mushroom_species);
  if ((selected_species != NULL) && (selected_species->description[0] != '\0')) {
    ShroomImGui_TextWrapped(selected_species->description);
    ShroomImGui_Text("Collection: server catalog loaded.");
  } else {
    ShroomImGui_TextWrapped("Connect to a server to load mushroom species catalog metadata.");
    ShroomImGui_Text("Collection: using offline fallback slots.");
  }

  ShroomImGui_Separator();
  ShroomImGui_Text("Keybindings");
  ShroomImGui_TextWrapped(
      "Click a slot, then press any key. Esc cancels. Enter and Tab are reserved.");

  if (ShroomImGui_BeginTable("KeybindingsTable", 2,
                             SHROOM_IMGUI_TABLE_BORDERS | SHROOM_IMGUI_TABLE_ROW_BG |
                                 SHROOM_IMGUI_TABLE_SIZING_FIXED,
                             530.0f, 160.0f)) {
    ShroomImGui_TableSetupColumn("Action", 320.0f);
    ShroomImGui_TableSetupColumn("Bound Key", 210.0f);

    for (int slot = 0; slot < SHROOM_KEYBINDING_SLOT_COUNT; ++slot) {
      int* slot_key = SettingsSlotKeyPtr(&g_settings_screen.pending, slot);
      const char* slot_label = ClientSettingsKeySlotLabel(slot);
      char button_label[64];

      ShroomImGui_TableNextRow();
      ShroomImGui_TableSetColumnIndex(0);
      ShroomImGui_Text(slot_label);
      ShroomImGui_TableSetColumnIndex(1);

      const bool is_rebinding =
          (g_settings_screen.rebind_slot == slot && g_settings_screen.rebind_armed);
      if (is_rebinding) {
        snprintf(button_label, sizeof(button_label), "Press a key##slot%d", slot);
        ShroomImGui_PushStyleColor(0, 0.95f, 0.7f, 0.25f, 1.0f);
      } else {
        snprintf(button_label, sizeof(button_label), "%s##slot%d",
                 ClientSettingsKeyLabel(*slot_key), slot);
      }
      if (ShroomImGui_Button(button_label, 160.0f, 0.0f)) {
        g_settings_screen.rebind_slot = slot;
        g_settings_screen.rebind_armed = true;
        g_settings_screen.rebind_conflict_slot = -1;
      }
      if (is_rebinding) {
        ShroomImGui_PopStyleColor();
      }
    }
    ShroomImGui_EndTable();
  }

  /* Surface conflict before save so the player fixes it first. */
  if (g_settings_screen.rebind_conflict_slot >= 0) {
    ShroomImGui_TextColored((ShroomImGuiColor){1.0f, 0.45f, 0.4f, 1.0f},
                            "Conflict: that key is bound to another slot.");
  }

  /* Rebind capture: if armed and not in an ImGui capture context, pull the
   * next queued key. Esc is the cancel key during rebind. */
  if (g_settings_screen.rebind_armed && !ShroomImGui_WantCaptureKeyboard()) {
    const int pressed = GetKeyPressed();
    if (pressed != 0) {
      if (pressed == KEY_ESCAPE) {
        g_settings_screen.rebind_slot = -1;
        g_settings_screen.rebind_armed = false;
      } else {
        const int conflicted = SettingsFindConflictSlot(&g_settings_screen.pending, pressed,
                                                        g_settings_screen.rebind_slot);
        if (conflicted >= 0) {
          g_settings_screen.rebind_conflict_slot = conflicted;
        } else {
          int* slot_key =
              SettingsSlotKeyPtr(&g_settings_screen.pending, g_settings_screen.rebind_slot);
          if (slot_key != NULL) {
            *slot_key = pressed;
            g_settings_screen.dirty = true;
            g_settings_screen.save_succeeded = false;
            ApplySettings(game);
          }
        }
        g_settings_screen.rebind_slot = -1;
        g_settings_screen.rebind_armed = false;
      }
    }
  }

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
    if (g_settings_screen.save_succeeded) {
      GamePlayUiClickSound(game);
    } else {
      GamePlayUiErrorSound(game);
    }
  }
  ShroomImGui_SameLine();
  if (ShroomImGui_Button("Back", 140.0f, 36.0f)) {
    GamePlayUiClickSound(game);
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
