#include "client_settings.h"

#include "raylib.h"
#include "shared/player_identity.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char* kClientSettingsPath = "client_settings.cfg";

void ClientSettingsSanitizePlayerName(char destination[SHROOM_MAX_NAME_LENGTH],
                                      const char* source) {
  ShroomSanitizePlayerName(destination, source);
}

bool ClientSettingsKeyIsReserved(int key) { return (key == KEY_ENTER) || (key == KEY_TAB); }

void ClientSettingsSetDefaults(ClientSettings* settings) {
  if (settings == NULL) {
    return;
  }

  *settings = (ClientSettings){
      .ui_scale_percent = 100,
      .master_volume_percent = 80,
      .music_volume_percent = 70,
      .effects_volume_percent = 85,
      .invert_mouse = false,
      .diagnostics_enabled = false,
      .show_ping_ms = true,
      .menu_animations_enabled = true,
      .death_cutscene_enabled = true,
      .camera_zoom = 1.0f,
      .preferred_region_index = 0,
      .palette_preset = CLIENT_PALETTE_CLASSIC,
      .hud_density = CLIENT_HUD_FULL,
      .particle_quality = CLIENT_PARTICLES_MEDIUM,
      .mushroom_species = CLIENT_MUSHROOM_AMANITA,
      .key_chat_open = KEY_T,
      .key_hud_toggle = KEY_F2,
      .key_pause_menu = KEY_ESCAPE,
      .key_push_to_talk = KEY_V,
  };
}

void ClientSettingsValidate(ClientSettings* settings) {
  char sanitized_name[SHROOM_MAX_NAME_LENGTH];
  if (settings == NULL) {
    return;
  }

  ClientSettingsSanitizePlayerName(sanitized_name, settings->player_name);
  snprintf(settings->player_name, sizeof(settings->player_name), "%s", sanitized_name);

  if ((settings->ui_scale_percent < 80) || (settings->ui_scale_percent > 160)) {
    settings->ui_scale_percent = 100;
  }
  if ((settings->master_volume_percent < 0) || (settings->master_volume_percent > 100)) {
    settings->master_volume_percent = 80;
  }
  if ((settings->music_volume_percent < 0) || (settings->music_volume_percent > 100)) {
    settings->music_volume_percent = 70;
  }
  if ((settings->effects_volume_percent < 0) || (settings->effects_volume_percent > 100)) {
    settings->effects_volume_percent = 85;
  }
  if ((settings->preferred_region_index < 0) || (settings->preferred_region_index > 2)) {
    settings->preferred_region_index = 0;
  }
  if ((settings->camera_zoom < 0.35f) || (settings->camera_zoom > 2.0f)) {
    settings->camera_zoom = 1.0f;
  }
  if ((settings->palette_preset < CLIENT_PALETTE_CLASSIC) ||
      (settings->palette_preset > CLIENT_PALETTE_HIGH_CONTRAST)) {
    settings->palette_preset = CLIENT_PALETTE_CLASSIC;
  }
  if ((settings->hud_density < CLIENT_HUD_FULL) || (settings->hud_density > CLIENT_HUD_MINIMAL)) {
    settings->hud_density = CLIENT_HUD_FULL;
  }
  if ((settings->particle_quality < CLIENT_PARTICLES_OFF) ||
      (settings->particle_quality > CLIENT_PARTICLES_HIGH)) {
    settings->particle_quality = CLIENT_PARTICLES_MEDIUM;
  }
  if ((settings->mushroom_species < CLIENT_MUSHROOM_AMANITA) ||
      (settings->mushroom_species >= CLIENT_MUSHROOM_COUNT)) {
    settings->mushroom_species = CLIENT_MUSHROOM_AMANITA;
  }
  /* Rebindable keys must be a valid raylib key (> KEY_NULL) and not one of
   * the reserved modal keys (Enter, Tab) — those are bound by UI contexts. */
  if (settings->key_chat_open <= KEY_NULL || ClientSettingsKeyIsReserved(settings->key_chat_open)) {
    settings->key_chat_open = KEY_T;
  }
  if (settings->key_hud_toggle <= KEY_NULL ||
      ClientSettingsKeyIsReserved(settings->key_hud_toggle)) {
    settings->key_hud_toggle = KEY_F2;
  }
  if (settings->key_pause_menu <= KEY_NULL ||
      ClientSettingsKeyIsReserved(settings->key_pause_menu)) {
    settings->key_pause_menu = KEY_ESCAPE;
  }
  if (settings->key_push_to_talk <= KEY_NULL ||
      ClientSettingsKeyIsReserved(settings->key_push_to_talk)) {
    settings->key_push_to_talk = KEY_V;
  }
}

bool ClientSettingsLoad(ClientSettings* settings) {
  FILE* file;
  char line[128];

  if (settings == NULL) {
    return false;
  }

  ClientSettingsSetDefaults(settings);

  file = fopen(kClientSettingsPath, "r");
  if (file == NULL) {
    return false;
  }

  while (fgets(line, sizeof(line), file) != NULL) {
    char key[48] = {0};
    int value = 0;

    if (strncmp(line, "player_name=", 12u) == 0) {
      char* name = line + 12u;
      name[strcspn(name, "\r\n")] = '\0';
      ClientSettingsSanitizePlayerName(settings->player_name, name);
      continue;
    }
    if (sscanf(line, "%47[^=]=%d", key, &value) != 2) {
      continue;
    }

    if (strcmp(key, "ui_scale_percent") == 0) {
      settings->ui_scale_percent = value;
    } else if (strcmp(key, "master_volume_percent") == 0) {
      settings->master_volume_percent = value;
    } else if (strcmp(key, "music_volume_percent") == 0) {
      settings->music_volume_percent = value;
    } else if (strcmp(key, "effects_volume_percent") == 0) {
      settings->effects_volume_percent = value;
    } else if (strcmp(key, "invert_mouse") == 0) {
      settings->invert_mouse = value != 0;
    } else if (strcmp(key, "diagnostics_enabled") == 0) {
      settings->diagnostics_enabled = value != 0;
    } else if (strcmp(key, "show_ping_ms") == 0) {
      settings->show_ping_ms = value != 0;
    } else if (strcmp(key, "menu_animations_enabled") == 0) {
      settings->menu_animations_enabled = value != 0;
    } else if (strcmp(key, "death_cutscene_enabled") == 0) {
      settings->death_cutscene_enabled = value != 0;
    } else if (strcmp(key, "preferred_region_index") == 0) {
      settings->preferred_region_index = value;
    } else if (strcmp(key, "palette_preset") == 0) {
      settings->palette_preset = (ClientPalettePreset)value;
    } else if (strcmp(key, "hud_density") == 0) {
      settings->hud_density = (ClientHudDensity)value;
    } else if (strcmp(key, "particle_quality") == 0) {
      settings->particle_quality = (ClientParticleQuality)value;
    } else if (strcmp(key, "mushroom_species") == 0) {
      settings->mushroom_species = (ClientMushroomSpecies)value;
    } else if (strcmp(key, "camera_zoom_x100") == 0) {
      settings->camera_zoom = (float)value / 100.0f;
    } else if (strcmp(key, "key_chat_open") == 0) {
      settings->key_chat_open = value;
    } else if (strcmp(key, "key_hud_toggle") == 0) {
      settings->key_hud_toggle = value;
    } else if (strcmp(key, "key_pause_menu") == 0) {
      settings->key_pause_menu = value;
    } else if (strcmp(key, "key_push_to_talk") == 0) {
      settings->key_push_to_talk = value;
    }
  }

  fclose(file);
  ClientSettingsValidate(settings);
  return true;
}

bool ClientSettingsSave(const ClientSettings* settings) {
  int file_descriptor;
  FILE* file;

  if (settings == NULL) {
    return false;
  }

  file_descriptor = open(kClientSettingsPath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (file_descriptor < 0) {
    return false;
  }

  file = fdopen(file_descriptor, "w");
  if (file == NULL) {
    close(file_descriptor);
    return false;
  }

  fprintf(file, "ui_scale_percent=%d\n", settings->ui_scale_percent);
  fprintf(file, "player_name=%s\n", settings->player_name);
  fprintf(file, "master_volume_percent=%d\n", settings->master_volume_percent);
  fprintf(file, "music_volume_percent=%d\n", settings->music_volume_percent);
  fprintf(file, "effects_volume_percent=%d\n", settings->effects_volume_percent);
  fprintf(file, "invert_mouse=%d\n", settings->invert_mouse ? 1 : 0);
  fprintf(file, "diagnostics_enabled=%d\n", settings->diagnostics_enabled ? 1 : 0);
  fprintf(file, "show_ping_ms=%d\n", settings->show_ping_ms ? 1 : 0);
  fprintf(file, "menu_animations_enabled=%d\n", settings->menu_animations_enabled ? 1 : 0);
  fprintf(file, "death_cutscene_enabled=%d\n", settings->death_cutscene_enabled ? 1 : 0);
  fprintf(file, "preferred_region_index=%d\n", settings->preferred_region_index);
  fprintf(file, "palette_preset=%d\n", (int)settings->palette_preset);
  fprintf(file, "hud_density=%d\n", (int)settings->hud_density);
  fprintf(file, "particle_quality=%d\n", (int)settings->particle_quality);
  fprintf(file, "mushroom_species=%d\n", (int)settings->mushroom_species);
  fprintf(file, "camera_zoom_x100=%d\n", (int)(settings->camera_zoom * 100.0f));
  fprintf(file, "key_chat_open=%d\n", settings->key_chat_open);
  fprintf(file, "key_hud_toggle=%d\n", settings->key_hud_toggle);
  fprintf(file, "key_pause_menu=%d\n", settings->key_pause_menu);
  fprintf(file, "key_push_to_talk=%d\n", settings->key_push_to_talk);

  fclose(file);
  return true;
}

const char* ClientSettingsPreferredRegionLabel(int region_index) {
  switch (region_index) {
  case 1:
    return "Europe";
  case 2:
    return "North America";
  case 0:
  default:
    return "Auto";
  }
}

const char* ClientSettingsPaletteLabel(ClientPalettePreset preset) {
  switch (preset) {
  case CLIENT_PALETTE_HIGH_CONTRAST:
    return "High Contrast";
  case CLIENT_PALETTE_CLASSIC:
  default:
    return "Classic";
  }
}

const char* ClientSettingsHudDensityLabel(ClientHudDensity density) {
  switch (density) {
  case CLIENT_HUD_COMPACT:
    return "Compact";
  case CLIENT_HUD_MINIMAL:
    return "Minimal";
  case CLIENT_HUD_FULL:
  default:
    return "Full";
  }
}

const char* ClientSettingsKeyLabel(int key) {
  /* Cover the small set of keys we actually expect; fall back to a
   * readable numeric label for anything else the user picks. */
  switch (key) {
  case KEY_NULL:
    return "Unbound";
  case KEY_ESCAPE:
    return "Esc";
  case KEY_ENTER:
    return "Enter";
  case KEY_TAB:
    return "Tab";
  case KEY_SPACE:
    return "Space";
  case KEY_BACKSPACE:
    return "Backspace";
  case KEY_INSERT:
    return "Insert";
  case KEY_DELETE:
    return "Delete";
  case KEY_HOME:
    return "Home";
  case KEY_END:
    return "End";
  case KEY_PAGE_UP:
    return "Page Up";
  case KEY_PAGE_DOWN:
    return "Page Down";
  case KEY_LEFT:
    return "Left";
  case KEY_RIGHT:
    return "Right";
  case KEY_UP:
    return "Up";
  case KEY_DOWN:
    return "Down";
  case KEY_F1:
    return "F1";
  case KEY_F2:
    return "F2";
  case KEY_F3:
    return "F3";
  case KEY_F4:
    return "F4";
  case KEY_F5:
    return "F5";
  case KEY_F6:
    return "F6";
  case KEY_F7:
    return "F7";
  case KEY_F8:
    return "F8";
  case KEY_F9:
    return "F9";
  case KEY_F10:
    return "F10";
  case KEY_F11:
    return "F11";
  case KEY_F12:
    return "F12";
  case KEY_LEFT_SHIFT:
    return "Left Shift";
  case KEY_LEFT_CONTROL:
    return "Left Ctrl";
  case KEY_LEFT_ALT:
    return "Left Alt";
  case KEY_LEFT_SUPER:
    return "Left Super";
  case KEY_RIGHT_SHIFT:
    return "Right Shift";
  case KEY_RIGHT_CONTROL:
    return "Right Ctrl";
  case KEY_RIGHT_ALT:
    return "Right Alt";
  case KEY_RIGHT_SUPER:
    return "Right Super";
  default:
    break;
  }

  static char fallback_buf[4][16];
  static int fallback_idx = 0;
  char* buf = fallback_buf[fallback_idx];
  fallback_idx = (fallback_idx + 1) % 4;
  if (key >= 32 && key <= 126) {
    snprintf(buf, 16, "%c", (char)key);
  } else {
    snprintf(buf, 16, "Key %d", key);
  }
  return buf;
}

const char* ClientSettingsKeySlotLabel(int slot_index) {
  switch (slot_index) {
  case 0:
    return "Chat Open";
  case 1:
    return "HUD Toggle";
  case 2:
    return "Pause / Menu";
  case 3:
    return "Push To Talk (reserved)";
  default:
    return "Unknown";
  }
}

bool ClientSettingsKeyIsBound(const ClientSettings* settings, int key) {
  if (settings == NULL || key <= KEY_NULL) {
    return false;
  }
  return settings->key_chat_open == key || settings->key_hud_toggle == key ||
         settings->key_pause_menu == key || settings->key_push_to_talk == key;
}
