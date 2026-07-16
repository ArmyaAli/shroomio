#include "client_settings.h"
#include "client_storage.h"

#include "raylib.h"
#include "shared/player_identity.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#endif

static const char* kClientSettingsLegacyPath = "client_settings.cfg";

static bool BuildDefaultSettingsPath(char* destination, size_t destination_size) {
  return ShroomClientStorageDefaultFile(destination, destination_size,
                                         SHROOM_CLIENT_STORAGE_CONFIG,
                                         "client_settings.cfg");
}

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
      .voice_enabled = true,
      .voice_self_muted = false,
      .voice_output_volume_percent = 80,
      .invert_mouse = false,
      .diagnostics_enabled = false,
      .show_ping_ms = true,
      .menu_animations_enabled = true,
      .death_cutscene_enabled = true,
      .account_features_enabled = false,
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
      .window_width = 1280,
      .window_height = 720,
      .fullscreen = false,
      .vsync = true,
      .input_sensitivity_percent = 100,
      .connection_type = CLIENT_CONNECTION_AUTO,
      .chat_visible = true,
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
  if ((settings->voice_output_volume_percent < 0) ||
      (settings->voice_output_volume_percent > 100)) {
    settings->voice_output_volume_percent = 80;
  }
  settings->voice_capture_device[sizeof(settings->voice_capture_device) - 1u] = '\0';
  settings->voice_capture_device[strcspn(settings->voice_capture_device, "\r\n")] = '\0';
  if ((settings->preferred_region_index < 0) || (settings->preferred_region_index > 2)) {
    settings->preferred_region_index = 0;
  }
  if ((settings->camera_zoom < 0.35f) || (settings->camera_zoom > 2.0f)) {
    settings->camera_zoom = 1.0f;
  }
  if ((settings->window_width < 640) || (settings->window_width > 7680)) {
    settings->window_width = 1280;
  }
  if ((settings->window_height < 360) || (settings->window_height > 4320)) {
    settings->window_height = 720;
  }
  if ((settings->input_sensitivity_percent < 25) ||
      (settings->input_sensitivity_percent > 200)) {
    settings->input_sensitivity_percent = 100;
  }
  if ((settings->connection_type < CLIENT_CONNECTION_AUTO) ||
      (settings->connection_type > CLIENT_CONNECTION_DIRECTORY)) {
    settings->connection_type = CLIENT_CONNECTION_AUTO;
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

enum {
  CLIENT_SETTINGS_LEGACY_FIELD_COUNT = 20,
  CLIENT_SETTINGS_PREVIOUS_FIELD_COUNT = 24,
  CLIENT_SETTINGS_V3_FIELD_COUNT = 25,
  CLIENT_SETTINGS_FIELD_COUNT = 32,
};
#define CLIENT_SETTINGS_LEGACY_MASK ((UINT64_C(1) << CLIENT_SETTINGS_LEGACY_FIELD_COUNT) - 1u)
#define CLIENT_SETTINGS_PREVIOUS_MASK ((UINT64_C(1) << CLIENT_SETTINGS_PREVIOUS_FIELD_COUNT) - 1u)
#define CLIENT_SETTINGS_V3_MASK ((UINT64_C(1) << CLIENT_SETTINGS_V3_FIELD_COUNT) - 1u)
#define CLIENT_SETTINGS_REQUIRED_MASK ((UINT64_C(1) << CLIENT_SETTINGS_FIELD_COUNT) - 1u)

static bool BuildSettingsSidePath(char* destination, size_t size, const char* path,
                                  const char* suffix) {
  int written;

  if ((destination == NULL) || (path == NULL) || (suffix == NULL)) {
    return false;
  }
  written = snprintf(destination, size, "%s%s", path, suffix);
  return (written >= 0) && ((size_t)written < size);
}

static bool ParseInteger(const char* text, int* value) {
  char* end;
  long parsed;

  if ((text == NULL) || (value == NULL) || (*text == '\0')) {
    return false;
  }
  errno = 0;
  parsed = strtol(text, &end, 10);
  if ((errno == ERANGE) || (*end != '\0')) {
    return false;
  }
  if ((parsed < INT_MIN) || (parsed > INT_MAX)) {
    return false;
  }
  *value = (int)parsed;
  return true;
}

static bool ParseSettingsFile(const char* path, ClientSettings* settings, bool* unversioned) {
  FILE* file;
  char line[160];
  uint64_t fields = 0u;
  int schema_version = 0;
  bool schema_seen = false;
  bool valid = true;

  ClientSettingsSetDefaults(settings);
  file = fopen(path, "rb");
  if (file == NULL) {
    return false;
  }
  while (fgets(line, sizeof(line), file) != NULL) {
    char* equals;
    char* key = line;
    const char* text;
    int value;
    unsigned int field = UINT_MAX;

    /* Every serialized record is newline-terminated; a missing terminator indicates truncation. */
    if (strchr(line, '\n') == NULL) {
      valid = false;
      break;
    }
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0') {
      continue;
    }
    equals = strchr(line, '=');
    if (equals == NULL) {
      valid = false;
      break;
    }
    *equals = '\0';
    text = equals + 1;
    /* The schema marker is optional only for the immediately preceding unversioned layout. */
    if (strcmp(key, "schema_version") == 0) {
      schema_seen = ParseInteger(text, &schema_version);
      valid = valid && schema_seen;
      continue;
    }
    /* Track each known field exactly once so partial, duplicate, and unknown records fail closed.
     */
    if (strcmp(key, "player_name") == 0) {
      ClientSettingsSanitizePlayerName(settings->player_name, text);
      field = 1u;
    } else if (strcmp(key, "voice_capture_device") == 0) {
      snprintf(settings->voice_capture_device, sizeof(settings->voice_capture_device), "%s", text);
      field = 23u;
    } else {
      if (!ParseInteger(text, &value)) {
        valid = false;
        break;
      }
      if (strcmp(key, "ui_scale_percent") == 0) {
        settings->ui_scale_percent = value;
        field = 0u;
      } else if (strcmp(key, "master_volume_percent") == 0) {
        settings->master_volume_percent = value;
        field = 2u;
      } else if (strcmp(key, "music_volume_percent") == 0) {
        settings->music_volume_percent = value;
        field = 3u;
      } else if (strcmp(key, "effects_volume_percent") == 0) {
        settings->effects_volume_percent = value;
        field = 4u;
      } else if (strcmp(key, "invert_mouse") == 0) {
        settings->invert_mouse = value != 0;
        field = 5u;
      } else if (strcmp(key, "diagnostics_enabled") == 0) {
        settings->diagnostics_enabled = value != 0;
        field = 6u;
      } else if (strcmp(key, "show_ping_ms") == 0) {
        settings->show_ping_ms = value != 0;
        field = 7u;
      } else if (strcmp(key, "menu_animations_enabled") == 0) {
        settings->menu_animations_enabled = value != 0;
        field = 8u;
      } else if (strcmp(key, "death_cutscene_enabled") == 0) {
        settings->death_cutscene_enabled = value != 0;
        field = 9u;
      } else if (strcmp(key, "preferred_region_index") == 0) {
        settings->preferred_region_index = value;
        field = 10u;
      } else if (strcmp(key, "palette_preset") == 0) {
        settings->palette_preset = (ClientPalettePreset)value;
        field = 11u;
      } else if (strcmp(key, "hud_density") == 0) {
        settings->hud_density = (ClientHudDensity)value;
        field = 12u;
      } else if (strcmp(key, "particle_quality") == 0) {
        settings->particle_quality = (ClientParticleQuality)value;
        field = 13u;
      } else if (strcmp(key, "mushroom_species") == 0) {
        settings->mushroom_species = (ClientMushroomSpecies)value;
        field = 14u;
      } else if (strcmp(key, "camera_zoom_x100") == 0) {
        settings->camera_zoom = (float)value / 100.0f;
        field = 15u;
      } else if (strcmp(key, "key_chat_open") == 0) {
        settings->key_chat_open = value;
        field = 16u;
      } else if (strcmp(key, "key_hud_toggle") == 0) {
        settings->key_hud_toggle = value;
        field = 17u;
      } else if (strcmp(key, "key_pause_menu") == 0) {
        settings->key_pause_menu = value;
        field = 18u;
      } else if (strcmp(key, "key_push_to_talk") == 0) {
        settings->key_push_to_talk = value;
        field = 19u;
      } else if (strcmp(key, "voice_enabled") == 0) {
        settings->voice_enabled = value != 0;
        field = 20u;
      } else if (strcmp(key, "voice_self_muted") == 0) {
        settings->voice_self_muted = value != 0;
        field = 21u;
      } else if (strcmp(key, "voice_output_volume_percent") == 0) {
        settings->voice_output_volume_percent = value;
        field = 22u;
      } else if (strcmp(key, "account_features_enabled") == 0) {
        settings->account_features_enabled = value != 0;
        field = 24u;
      } else if (strcmp(key, "window_width") == 0) {
        settings->window_width = value;
        field = 25u;
      } else if (strcmp(key, "window_height") == 0) {
        settings->window_height = value;
        field = 26u;
      } else if (strcmp(key, "fullscreen") == 0) {
        settings->fullscreen = value != 0;
        field = 27u;
      } else if (strcmp(key, "vsync") == 0) {
        settings->vsync = value != 0;
        field = 28u;
      } else if (strcmp(key, "input_sensitivity_percent") == 0) {
        settings->input_sensitivity_percent = value;
        field = 29u;
      } else if (strcmp(key, "connection_type") == 0) {
        settings->connection_type = (ClientConnectionType)value;
        field = 30u;
      } else if (strcmp(key, "chat_visible") == 0) {
        settings->chat_visible = value != 0;
        field = 31u;
      }
    }
    if (field < CLIENT_SETTINGS_FIELD_COUNT) {
      fields |= 1u << field;
    }
  }
  if (ferror(file)) {
    valid = false;
  }
  fclose(file);

  const bool current_schema = schema_seen && (schema_version == CLIENT_SETTINGS_SCHEMA_VERSION) &&
                              (fields == CLIENT_SETTINGS_REQUIRED_MASK);
  const bool unversioned_schema = !schema_seen && ((fields == CLIENT_SETTINGS_LEGACY_MASK) ||
                                                   (fields == CLIENT_SETTINGS_PREVIOUS_MASK) ||
                                                   (fields == CLIENT_SETTINGS_REQUIRED_MASK));
  const bool schema_one =
      schema_seen && (schema_version == 1) && (fields == CLIENT_SETTINGS_LEGACY_MASK);
  const bool schema_two =
      schema_seen && (schema_version == 2) &&
      ((fields & CLIENT_SETTINGS_PREVIOUS_MASK) == CLIENT_SETTINGS_PREVIOUS_MASK);
  const bool schema_three =
      schema_seen && (schema_version == 3) && (fields == CLIENT_SETTINGS_V3_MASK);
  valid = valid && (current_schema || unversioned_schema || schema_one || schema_two || schema_three);
  if (!valid) {
    ClientSettingsSetDefaults(settings);
    return false;
  }
  ClientSettingsValidate(settings);
  if (unversioned != NULL) {
    *unversioned = !schema_seen || (schema_version != CLIENT_SETTINGS_SCHEMA_VERSION);
  }
  return true;
}

static bool FlushSettingsFile(FILE* file) {
  if ((fflush(file) != 0) || (ferror(file) != 0)) {
    return false;
  }
#ifdef _WIN32
  return _commit(_fileno(file)) == 0;
#else
  return fsync(fileno(file)) == 0;
#endif
}

static bool ReplaceSettingsFile(const char* source, const char* destination) {
#ifdef _WIN32
  return MoveFileExA(source, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  return rename(source, destination) == 0;
#endif
}

static FILE* OpenPrivateSettingsFile(const char* path) {
  int descriptor;
  FILE* file;

#ifdef _WIN32
  descriptor =
      _open(path, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY | _O_NOINHERIT, _S_IREAD | _S_IWRITE);
#else
  descriptor = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if ((descriptor >= 0) && (fchmod(descriptor, S_IRUSR | S_IWUSR) != 0)) {
    close(descriptor);
    remove(path);
    return NULL;
  }
#endif
  if (descriptor < 0) {
    return NULL;
  }
#ifdef _WIN32
  file = _fdopen(descriptor, "wb");
#else
  file = fdopen(descriptor, "wb");
#endif
  if (file == NULL) {
#ifdef _WIN32
    _close(descriptor);
#else
    close(descriptor);
#endif
    remove(path);
  }
  return file;
}

static bool WriteSettingsFile(const char* path, const ClientSettings* settings) {
  FILE* file = OpenPrivateSettingsFile(path);
  bool success;

  if (file == NULL) {
    return false;
  }
  success =
      fprintf(file, "schema_version=%d\n", CLIENT_SETTINGS_SCHEMA_VERSION) >= 0 &&
      fprintf(file, "ui_scale_percent=%d\n", settings->ui_scale_percent) >= 0 &&
      fprintf(file, "player_name=%s\n", settings->player_name) >= 0 &&
      fprintf(file, "master_volume_percent=%d\n", settings->master_volume_percent) >= 0 &&
      fprintf(file, "music_volume_percent=%d\n", settings->music_volume_percent) >= 0 &&
      fprintf(file, "effects_volume_percent=%d\n", settings->effects_volume_percent) >= 0 &&
      fprintf(file, "voice_enabled=%d\n", settings->voice_enabled ? 1 : 0) >= 0 &&
      fprintf(file, "voice_self_muted=%d\n", settings->voice_self_muted ? 1 : 0) >= 0 &&
      fprintf(file, "voice_output_volume_percent=%d\n", settings->voice_output_volume_percent) >=
          0 &&
      fprintf(file, "voice_capture_device=%s\n", settings->voice_capture_device) >= 0 &&
      fprintf(file, "invert_mouse=%d\n", settings->invert_mouse ? 1 : 0) >= 0 &&
      fprintf(file, "diagnostics_enabled=%d\n", settings->diagnostics_enabled ? 1 : 0) >= 0 &&
      fprintf(file, "show_ping_ms=%d\n", settings->show_ping_ms ? 1 : 0) >= 0 &&
      fprintf(file, "menu_animations_enabled=%d\n", settings->menu_animations_enabled ? 1 : 0) >=
          0 &&
      fprintf(file, "death_cutscene_enabled=%d\n", settings->death_cutscene_enabled ? 1 : 0) >= 0 &&
      fprintf(file, "account_features_enabled=%d\n", settings->account_features_enabled ? 1 : 0) >=
          0 &&
      fprintf(file, "preferred_region_index=%d\n", settings->preferred_region_index) >= 0 &&
      fprintf(file, "palette_preset=%d\n", (int)settings->palette_preset) >= 0 &&
      fprintf(file, "hud_density=%d\n", (int)settings->hud_density) >= 0 &&
      fprintf(file, "particle_quality=%d\n", (int)settings->particle_quality) >= 0 &&
      fprintf(file, "mushroom_species=%d\n", (int)settings->mushroom_species) >= 0 &&
      fprintf(file, "camera_zoom_x100=%d\n", (int)(settings->camera_zoom * 100.0f)) >= 0 &&
      fprintf(file, "key_chat_open=%d\n", settings->key_chat_open) >= 0 &&
      fprintf(file, "key_hud_toggle=%d\n", settings->key_hud_toggle) >= 0 &&
      fprintf(file, "key_pause_menu=%d\n", settings->key_pause_menu) >= 0 &&
      fprintf(file, "key_push_to_talk=%d\n", settings->key_push_to_talk) >= 0;
  success = success && fprintf(file, "window_width=%d\n", settings->window_width) >= 0 &&
            fprintf(file, "window_height=%d\n", settings->window_height) >= 0 &&
            fprintf(file, "fullscreen=%d\n", settings->fullscreen ? 1 : 0) >= 0 &&
            fprintf(file, "vsync=%d\n", settings->vsync ? 1 : 0) >= 0 &&
            fprintf(file, "input_sensitivity_percent=%d\n", settings->input_sensitivity_percent) >= 0 &&
            fprintf(file, "connection_type=%d\n", (int)settings->connection_type) >= 0 &&
            fprintf(file, "chat_visible=%d\n", settings->chat_visible ? 1 : 0) >= 0;
  success = success && FlushSettingsFile(file);
  if (fclose(file) != 0) {
    success = false;
  }
  if (!success) {
    remove(path);
  }
  return success;
}

static bool CopyFileContents(const char* source, const char* destination) {
  FILE* input = fopen(source, "rb");
  FILE* output;
  char buffer[4096];
  bool success = true;

  if (input == NULL) {
    return false;
  }
  output = OpenPrivateSettingsFile(destination);
  if (output == NULL) {
    fclose(input);
    return false;
  }
  while (!feof(input)) {
    size_t count = fread(buffer, 1u, sizeof(buffer), input);
    if ((count > 0u) && (fwrite(buffer, 1u, count, output) != count)) {
      success = false;
      break;
    }
    if (ferror(input)) {
      success = false;
      break;
    }
  }
  success = success && FlushSettingsFile(output);
  if ((fclose(input) != 0) || (fclose(output) != 0)) {
    success = false;
  }
  if (!success) {
    remove(destination);
  }
  return success;
}

bool ClientSettingsSaveToPath(const ClientSettings* settings, const char* path) {
  ClientSettings validated;
  ClientSettings parsed;
  char temporary[512];
  char backup[512];
  char backup_temporary[512];
  bool ignored;

  if ((settings == NULL) || (path == NULL) || (*path == '\0') ||
      !BuildSettingsSidePath(temporary, sizeof(temporary), path, ".tmp") ||
      !BuildSettingsSidePath(backup, sizeof(backup), path, ".bak") ||
      !BuildSettingsSidePath(backup_temporary, sizeof(backup_temporary), path, ".bak.tmp")) {
    return false;
  }
  validated = *settings;
  ClientSettingsValidate(&validated);
  remove(temporary);
  if (!WriteSettingsFile(temporary, &validated) ||
      !ParseSettingsFile(temporary, &parsed, &ignored)) {
    remove(temporary);
    return false;
  }

  if (ParseSettingsFile(path, &parsed, &ignored)) {
    remove(backup_temporary);
    if (!CopyFileContents(path, backup_temporary) ||
        !ParseSettingsFile(backup_temporary, &parsed, &ignored) ||
        !ReplaceSettingsFile(backup_temporary, backup)) {
      remove(temporary);
      remove(backup_temporary);
      return false;
    }
  }
  if (!ReplaceSettingsFile(temporary, path)) {
    remove(temporary);
    return false;
  }
  return true;
}

bool ClientSettingsLoadFromPath(ClientSettings* settings, const char* path) {
  char backup[512];
  bool unversioned = false;

  if ((settings == NULL) || (path == NULL) || (*path == '\0') ||
      !BuildSettingsSidePath(backup, sizeof(backup), path, ".bak")) {
    if (settings != NULL) {
      ClientSettingsSetDefaults(settings);
      ClientSettingsValidate(settings);
    }
    return false;
  }
  if (ParseSettingsFile(path, settings, &unversioned)) {
    if (unversioned) {
      (void)ClientSettingsSaveToPath(settings, path);
    }
    return true;
  }
  if (ParseSettingsFile(backup, settings, &unversioned)) {
    (void)ClientSettingsSaveToPath(settings, path);
    return true;
  }
  ClientSettingsSetDefaults(settings);
  ClientSettingsValidate(settings);
  return false;
}

bool ClientSettingsLoad(ClientSettings* settings) {
  char path[SHROOM_CLIENT_STORAGE_PATH_MAX];
  if (!BuildDefaultSettingsPath(path, sizeof(path))) {
    return ClientSettingsLoadFromPath(settings, kClientSettingsLegacyPath);
  }
  if (ClientSettingsLoadFromPath(settings, path)) {
    return true;
  }
  if (ClientSettingsLoadFromPath(settings, kClientSettingsLegacyPath)) {
    if (ClientSettingsSaveToPath(settings, path)) {
      remove(kClientSettingsLegacyPath);
    }
    return true;
  }
  ClientSettingsSetDefaults(settings);
  ClientSettingsValidate(settings);
  return false;
}

bool ClientSettingsSave(const ClientSettings* settings) {
  char path[SHROOM_CLIENT_STORAGE_PATH_MAX];
  return BuildDefaultSettingsPath(path, sizeof(path)) && ClientSettingsSaveToPath(settings, path);
}

bool ClientSettingsCommitPlayerNameToPath(ClientSettings* settings, const char* player_name,
                                          const char* path) {
  ClientSettings pending;

  if ((settings == NULL) || (player_name == NULL)) {
    return false;
  }
  pending = *settings;
  ClientSettingsSanitizePlayerName(pending.player_name, player_name);
  if ((pending.player_name[0] == '\0') || !ClientSettingsSaveToPath(&pending, path)) {
    return false;
  }
  *settings = pending;
  return true;
}

bool ClientSettingsCommitPlayerName(ClientSettings* settings, const char* player_name) {
  char path[SHROOM_CLIENT_STORAGE_PATH_MAX];
  return BuildDefaultSettingsPath(path, sizeof(path)) &&
         ClientSettingsCommitPlayerNameToPath(settings, player_name, path);
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
