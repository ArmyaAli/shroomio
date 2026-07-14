#include "game.h"
#include "audio.h"
#include "layout.h"
#include "screen.h"
#include "screen_background.h"
#include "settings_session.h"
#include "voice.h"

#include "imgui_wrapper.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>

#define SHROOM_KEYBINDING_SLOT_COUNT 4

typedef struct SettingsScreenState {
  ShroomSettingsSession session;
  bool save_succeeded;
  bool save_failed;
  bool confirm_restore_defaults;
  /* Rebind capture state. slot == -1 means idle; 0..3 captures next key
   * for that slot. The captured-key path uses a single-frame queue so
   * the press that opens the rebind UI does not also assign. */
  int rebind_slot;
  bool rebind_armed;
  int rebind_conflict_slot; /* -1 if no conflict, else the slot that owns the pressed key */
  int rebind_reserved_key;  /* KEY_NULL if the last captured key was assignable */
  char capture_devices[SHROOM_VOICE_MAX_CAPTURE_DEVICES + 1u][SHROOM_VOICE_DEVICE_NAME_LENGTH];
  size_t capture_device_count;
  int selected_capture_device;
  bool capture_device_fallback;
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

static void ApplySettings(Game* game, const ClientSettings* settings) {
  game->settings = *settings;
  ShroomClientAudioInit(&game->settings);
  ShroomVoiceSetSelfMuted(game->settings.voice_self_muted);
  ShroomVoiceSetOutputVolume(game->settings.voice_output_volume_percent);
  (void)ShroomVoiceSelectCaptureDevice(game->settings.voice_capture_device);
}

static void DiscardAndGoBack(ShroomScreenManager* manager) {
  ShroomSettingsSessionDiscard(&g_settings_screen.session);
  ShroomScreenManagerGoBack(manager);
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

  ShroomSettingsSessionInit(&g_settings_screen.session, &game->settings);
  g_settings_screen.save_succeeded = false;
  g_settings_screen.save_failed = false;
  g_settings_screen.confirm_restore_defaults = false;
  g_settings_screen.rebind_slot = -1;
  g_settings_screen.rebind_armed = false;
  g_settings_screen.rebind_conflict_slot = -1;
  g_settings_screen.rebind_reserved_key = KEY_NULL;
  snprintf(g_settings_screen.capture_devices[0], sizeof(g_settings_screen.capture_devices[0]), "%s",
           "System Default");
  g_settings_screen.capture_device_count =
      1u + ShroomVoiceListCaptureDevices(&g_settings_screen.capture_devices[1],
                                         SHROOM_VOICE_MAX_CAPTURE_DEVICES);
  g_settings_screen.selected_capture_device = 0;
  g_settings_screen.capture_device_fallback = game->settings.voice_capture_device[0] != '\0';
  for (size_t index = 1u; index < g_settings_screen.capture_device_count; ++index) {
    if (strcmp(game->settings.voice_capture_device, g_settings_screen.capture_devices[index]) ==
        0) {
      g_settings_screen.selected_capture_device = (int)index;
      g_settings_screen.capture_device_fallback = false;
      break;
    }
  }
  return true;
}

static void SettingsCaptureKey(int pressed) {
  int conflicted;

  if (!g_settings_screen.rebind_armed || (pressed == KEY_NULL)) {
    return;
  }
  if (pressed == KEY_ESCAPE) {
    g_settings_screen.rebind_slot = -1;
    g_settings_screen.rebind_armed = false;
    g_settings_screen.rebind_conflict_slot = -1;
    g_settings_screen.rebind_reserved_key = KEY_NULL;
    return;
  }
  if (ClientSettingsKeyIsReserved(pressed)) {
    g_settings_screen.rebind_conflict_slot = -1;
    g_settings_screen.rebind_reserved_key = pressed;
    return;
  }

  conflicted = SettingsFindConflictSlot(&g_settings_screen.session.pending, pressed,
                                        g_settings_screen.rebind_slot);
  if (conflicted >= 0) {
    g_settings_screen.rebind_conflict_slot = conflicted;
  } else {
    int* slot_key =
        SettingsSlotKeyPtr(&g_settings_screen.session.pending, g_settings_screen.rebind_slot);
    if (slot_key != NULL) {
      *slot_key = pressed;
      ShroomSettingsSessionMarkDirty(&g_settings_screen.session);
      g_settings_screen.save_succeeded = false;
      g_settings_screen.save_failed = false;
    }
  }
  g_settings_screen.rebind_reserved_key = KEY_NULL;
  g_settings_screen.rebind_slot = -1;
  g_settings_screen.rebind_armed = false;
}

static const char* SettingsRebindErrorText(void) {
  if (g_settings_screen.rebind_reserved_key != KEY_NULL) {
    static char message[128];
    snprintf(message, sizeof(message), "%s is reserved for interface controls. Choose another key.",
             ClientSettingsKeyLabel(g_settings_screen.rebind_reserved_key));
    return message;
  }
  if (g_settings_screen.rebind_conflict_slot >= 0) {
    return "Conflict: that key is bound to another slot.";
  }
  return "";
}

static void SettingsDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  bool changed = false;
  const char* species_items[CLIENT_MUSHROOM_COUNT];
  char species_fallback_items[CLIENT_MUSHROOM_COUNT][24];
  const ShroomMushroomSpeciesEntry* selected_species;
  const char* capture_device_items[SHROOM_VOICE_MAX_CAPTURE_DEVICES + 1u];
  const int screen_width = GetScreenWidth();
  const int screen_height = GetScreenHeight();

  if (game == NULL) {
    return;
  }

  for (size_t index = 0u; index < g_settings_screen.capture_device_count; ++index) {
    capture_device_items[index] = g_settings_screen.capture_devices[index];
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

  ShroomImGui_BeginChild("SettingsContent", 0.0f, (float)screen_height * 0.8f - 150.0f, false);
  ShroomImGui_Text("Identity");
  ShroomLayoutSetNextLabeledItemWidth("Player Name");
  changed |= ShroomImGui_InputText("Player Name", g_settings_screen.session.pending.player_name,
                                   sizeof(g_settings_screen.session.pending.player_name));

  ShroomImGui_Separator();
  ShroomImGui_Text("Interface");
  changed |= ShroomImGui_SliderInt("UI Scale", &g_settings_screen.session.pending.ui_scale_percent,
                                   80, 160, "%d%%");
  changed |=
      ShroomImGui_Combo("Preferred Region",
                        &g_settings_screen.session.pending.preferred_region_index, kRegionItems, 3);
  changed |= ShroomImGui_Combo("Palette", (int*)&g_settings_screen.session.pending.palette_preset,
                               kPaletteItems, 2);

  ShroomImGui_Separator();
  ShroomImGui_Text("Audio");
  changed |= ShroomImGui_SliderInt(
      "Master Volume", &g_settings_screen.session.pending.master_volume_percent, 0, 100, "%d%%");
  changed |= ShroomImGui_SliderInt(
      "Music Volume", &g_settings_screen.session.pending.music_volume_percent, 0, 100, "%d%%");
  changed |= ShroomImGui_SliderInt(
      "Effects Volume", &g_settings_screen.session.pending.effects_volume_percent, 0, 100, "%d%%");
  if (ShroomImGui_Button("Restart Audio", 160.0f, 32.0f)) {
    ShroomClientAudioRestart(&game->settings);
  }
  ShroomImGui_SameLine();
  if (ShroomClientAudioIsReady()) {
    ShroomImGui_Text("Audio ready");
  } else {
    ShroomImGui_TextColored((ShroomImGuiColor){1.0f, 0.45f, 0.4f, 1.0f},
                            ShroomClientAudioGetStatus());
  }

  ShroomImGui_Separator();
  ShroomImGui_Text("Voice Chat");
  changed |=
      ShroomImGui_Checkbox("Enable Voice Chat", &g_settings_screen.session.pending.voice_enabled);
  changed |= ShroomImGui_Checkbox("Self Mute", &g_settings_screen.session.pending.voice_self_muted);
  changed |= ShroomImGui_SliderInt("Voice Output Volume",
                                   &g_settings_screen.session.pending.voice_output_volume_percent,
                                   0, 100, "%d%%");
  if (ShroomImGui_Combo("Capture Device", &g_settings_screen.selected_capture_device,
                        capture_device_items, (int)g_settings_screen.capture_device_count)) {
    const int selected = g_settings_screen.selected_capture_device;
    snprintf(g_settings_screen.session.pending.voice_capture_device,
             sizeof(g_settings_screen.session.pending.voice_capture_device), "%s",
             selected > 0 ? g_settings_screen.capture_devices[selected] : "");
    g_settings_screen.capture_device_fallback = false;
    changed = true;
  }
  if (g_settings_screen.capture_device_fallback) {
    ShroomImGui_TextColored((ShroomImGuiColor){1.0f, 0.65f, 0.3f, 1.0f},
                            "Saved capture device unavailable; using System Default.");
  } else if (g_settings_screen.capture_device_count == 1u) {
    ShroomImGui_TextColored((ShroomImGuiColor){1.0f, 0.65f, 0.3f, 1.0f},
                            "No capture devices detected; System Default will be retried.");
  }
  if (ShroomVoiceIsMicTestActive()) {
    ShroomVoiceUpdate(false, NULL, NULL);
    if (ShroomImGui_Button("Stop Microphone Test", 190.0f, 32.0f)) {
      ShroomVoiceEndMicTest();
    }
    ShroomImGui_SameLine();
    ShroomImGui_Text(TextFormat("Input level: %d%%", (int)(ShroomVoiceGetCaptureLevel() * 100.0f)));
  } else if (ShroomImGui_Button(((ShroomVoiceGetStatus() == SHROOM_VOICE_STATUS_ERROR) ||
                                 (ShroomVoiceGetStatus() == SHROOM_VOICE_STATUS_RECOVERING))
                                    ? "Retry Microphone"
                                    : "Test Microphone",
                                170.0f, 32.0f)) {
    (void)ShroomVoiceSelectCaptureDevice(g_settings_screen.session.pending.voice_capture_device);
    (void)ShroomVoiceBeginMicTest();
  }
  if ((ShroomVoiceGetStatus() == SHROOM_VOICE_STATUS_RECOVERING) ||
      (ShroomVoiceGetStatus() == SHROOM_VOICE_STATUS_ERROR) ||
      (ShroomVoiceGetStatus() == SHROOM_VOICE_STATUS_UNCONFIGURED)) {
    ShroomImGui_TextColored((ShroomImGuiColor){1.0f, 0.45f, 0.4f, 1.0f},
                            ShroomVoiceGetStatusText());
  }

  ShroomImGui_Separator();
  ShroomImGui_Text("Gameplay");
  changed |= ShroomImGui_Checkbox("Invert Mouse", &g_settings_screen.session.pending.invert_mouse);
  changed |= ShroomImGui_Checkbox("Show Diagnostics On Launch",
                                  &g_settings_screen.session.pending.diagnostics_enabled);
  changed |=
      ShroomImGui_Checkbox("Show Ping In HUD", &g_settings_screen.session.pending.show_ping_ms);
  changed |= ShroomImGui_Combo("Particle Quality",
                               (int*)&g_settings_screen.session.pending.particle_quality,
                               kParticleItems, 4);
  changed |= ShroomImGui_Checkbox("Animated Menu Backgrounds",
                                  &g_settings_screen.session.pending.menu_animations_enabled);
  changed |= ShroomImGui_Checkbox("Death Cutscene",
                                  &g_settings_screen.session.pending.death_cutscene_enabled);
  changed |= ShroomImGui_Combo("Mushroom Species",
                               (int*)&g_settings_screen.session.pending.mushroom_species,
                               species_items, CLIENT_MUSHROOM_COUNT);
  selected_species =
      FindSpeciesEntry(&game->net, g_settings_screen.session.pending.mushroom_species);
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
      int* slot_key = SettingsSlotKeyPtr(&g_settings_screen.session.pending, slot);
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
        g_settings_screen.rebind_reserved_key = KEY_NULL;
      }
      if (is_rebinding) {
        ShroomImGui_PopStyleColor();
      }
    }
    ShroomImGui_EndTable();
  }

  /* Surface conflict before save so the player fixes it first. */
  if (SettingsRebindErrorText()[0] != '\0') {
    ShroomImGui_TextColored((ShroomImGuiColor){1.0f, 0.45f, 0.4f, 1.0f}, SettingsRebindErrorText());
  }

  /* Rebind capture: if armed and not in an ImGui capture context, pull the
   * next queued key. Esc is the cancel key during rebind. */
  if (g_settings_screen.rebind_armed && !ShroomImGui_WantCaptureKeyboard()) {
    const int pressed = GetKeyPressed();
    SettingsCaptureKey(pressed);
  }

  ShroomImGui_EndChild();

  if (changed) {
    ClientSettingsValidate(&g_settings_screen.session.pending);
    ShroomSettingsSessionMarkDirty(&g_settings_screen.session);
    g_settings_screen.save_succeeded = false;
    g_settings_screen.save_failed = false;
  }

  ShroomImGui_Spacing();
  if (g_settings_screen.save_succeeded) {
    ShroomImGui_Text("Settings applied and saved to client_settings.cfg");
  } else if (g_settings_screen.save_failed) {
    ShroomImGui_TextColored((ShroomImGuiColor){1.0f, 0.45f, 0.4f, 1.0f},
                            "Save failed; current settings were not changed.");
  } else if (g_settings_screen.session.dirty) {
    ShroomImGui_Text("Unsaved changes; Save to apply them.");
  }

  if (ShroomImGui_Button("Save", 140.0f, 36.0f)) {
    ClientSettingsValidate(&g_settings_screen.session.pending);
    g_settings_screen.save_succeeded = ClientSettingsSave(&g_settings_screen.session.pending);
    g_settings_screen.save_failed = !g_settings_screen.save_succeeded;
    if (g_settings_screen.save_succeeded) {
      ApplySettings(game, &g_settings_screen.session.pending);
      ShroomSettingsSessionCommit(&g_settings_screen.session);
      GamePlayUiClickSound(game);
    } else {
      GamePlayUiErrorSound(game);
    }
  }
  ShroomImGui_SameLine();
  if (ShroomImGui_Button(g_settings_screen.session.dirty ? "Discard Changes" : "Back", 160.0f,
                         36.0f)) {
    GamePlayUiClickSound(game);
    DiscardAndGoBack(manager);
  }
  ShroomImGui_SameLine();
  if (ShroomImGui_Button("Restore Defaults", 160.0f, 36.0f)) {
    g_settings_screen.confirm_restore_defaults = true;
    g_settings_screen.save_succeeded = false;
    g_settings_screen.save_failed = false;
  }

  if (g_settings_screen.confirm_restore_defaults) {
    ShroomImGui_Text("Replace pending values with defaults?");
    if (ShroomImGui_Button("Confirm Defaults", 160.0f, 32.0f)) {
      ShroomSettingsSessionRestoreDefaults(&g_settings_screen.session);
      g_settings_screen.confirm_restore_defaults = false;
    }
    ShroomImGui_SameLine();
    if (ShroomImGui_Button("Keep Current", 140.0f, 32.0f)) {
      g_settings_screen.confirm_restore_defaults = false;
    }
  }

  ShroomImGui_End();
}

static void HandleSettingsEscape(ShroomScreenManager* manager) {
  if (g_settings_screen.rebind_armed) {
    SettingsCaptureKey(KEY_ESCAPE);
  } else {
    DiscardAndGoBack(manager);
  }
}

#ifdef TEST_MODE
void ShroomTestSettingsEscape(ShroomScreenManager* manager) { HandleSettingsEscape(manager); }

void ShroomTestSettingsBeginRebind(int slot) {
  if (SettingsSlotKeyPtr(&g_settings_screen.session.pending, slot) == NULL) {
    return;
  }
  g_settings_screen.rebind_slot = slot;
  g_settings_screen.rebind_armed = true;
  g_settings_screen.rebind_conflict_slot = -1;
  g_settings_screen.rebind_reserved_key = KEY_NULL;
}

void ShroomTestSettingsCaptureKey(int key) { SettingsCaptureKey(key); }

int ShroomTestSettingsPendingKey(int slot) {
  int* key = SettingsSlotKeyPtr(&g_settings_screen.session.pending, slot);
  return key != NULL ? *key : KEY_NULL;
}

const char* ShroomTestGetSettingsRebindError(void) { return SettingsRebindErrorText(); }

bool ShroomTestSettingsUsesCaptureFallback(void) {
  return g_settings_screen.capture_device_fallback;
}
#endif

static void SettingsHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    HandleSettingsEscape(manager);
  }
}

static void SettingsCleanup(ShroomScreenManager* manager) {
  (void)manager;
  ShroomVoiceEndMicTest();
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
  screen->cleanup = SettingsCleanup;
}
