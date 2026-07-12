#ifndef SHROOM_CLIENT_SETTINGS_H
#define SHROOM_CLIENT_SETTINGS_H

#include <stdbool.h>

#include "shared/config.h"

typedef enum ClientPalettePreset {
  CLIENT_PALETTE_CLASSIC = 0,
  CLIENT_PALETTE_HIGH_CONTRAST,
} ClientPalettePreset;

typedef enum ClientHudDensity {
  CLIENT_HUD_FULL = 0,
  CLIENT_HUD_COMPACT,
  CLIENT_HUD_MINIMAL,
} ClientHudDensity;

typedef enum ClientParticleQuality {
  CLIENT_PARTICLES_OFF = 0,
  CLIENT_PARTICLES_LOW,
  CLIENT_PARTICLES_MEDIUM,
  CLIENT_PARTICLES_HIGH,
} ClientParticleQuality;

typedef enum ClientMushroomSpecies {
  CLIENT_MUSHROOM_AMANITA = 0,
  CLIENT_MUSHROOM_CHANTERELLE,
  CLIENT_MUSHROOM_MOREL,
  CLIENT_MUSHROOM_SHIITAKE,
  CLIENT_MUSHROOM_OYSTER,
  CLIENT_MUSHROOM_ENOKI,
  CLIENT_MUSHROOM_PORTOBELLO,
  CLIENT_MUSHROOM_LIONS_MANE,
  CLIENT_MUSHROOM_REISHI,
  CLIENT_MUSHROOM_BLEWIT,
  CLIENT_MUSHROOM_COUNT,
} ClientMushroomSpecies;

typedef struct ClientSettings {
  char player_name[SHROOM_MAX_NAME_LENGTH];
  int ui_scale_percent;
  int master_volume_percent;
  int music_volume_percent;
  int effects_volume_percent;
  bool invert_mouse;
  bool diagnostics_enabled;
  bool show_ping_ms;
  bool menu_animations_enabled;
  bool death_cutscene_enabled;
  float camera_zoom;
  int preferred_region_index;
  ClientPalettePreset palette_preset;
  ClientHudDensity hud_density;
  ClientParticleQuality particle_quality;
  ClientMushroomSpecies mushroom_species;
  /* Rebindable hotkeys (raylib KeyboardKey ints). Push-to-talk is a
   * reserved slot — no behavior yet, but the binding persists and shows
   * in the keybinding UI so future voice work can read it directly. */
  int key_chat_open;
  int key_hud_toggle;
  int key_pause_menu;
  int key_push_to_talk;
} ClientSettings;

void ClientSettingsSetDefaults(ClientSettings* settings);
void ClientSettingsValidate(ClientSettings* settings);
bool ClientSettingsLoad(ClientSettings* settings);
bool ClientSettingsSave(const ClientSettings* settings);
const char* ClientSettingsPreferredRegionLabel(int region_index);
const char* ClientSettingsPaletteLabel(ClientPalettePreset preset);
const char* ClientSettingsHudDensityLabel(ClientHudDensity density);
const char* ClientSettingsKeyLabel(int key);
const char* ClientSettingsKeySlotLabel(int slot_index);
bool ClientSettingsKeyIsBound(const ClientSettings* settings, int key);
bool ClientSettingsKeyIsReserved(int key);
void ClientSettingsSanitizePlayerName(char destination[SHROOM_MAX_NAME_LENGTH], const char* source);

#endif
