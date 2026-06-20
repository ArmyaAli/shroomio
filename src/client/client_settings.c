#include "client_settings.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char* kClientSettingsPath = "client_settings.cfg";

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
  };
}

void ClientSettingsValidate(ClientSettings* settings) {
  if (settings == NULL) {
    return;
  }

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
