#include "audio.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "raymath.h"

static Sound g_client_sfx[SHROOM_CLIENT_SFX_COUNT];
static bool g_client_sfx_loaded[SHROOM_CLIENT_SFX_COUNT];
static Sound g_client_music_loop;
static bool g_client_music_loaded;

static float SmoothStep01(float value) {
  const float t = Clamp(value, 0.0f, 1.0f);
  return t * t * (3.0f - (2.0f * t));
}

static float NextNoiseSample(uint32_t* state) {
  *state = (*state * 1664525u) + 1013904223u;
  return ((float)((*state >> 8u) & 0xFFFFu) / 32767.5f) - 1.0f;
}

static Sound GenerateOrganicSfx(float start_hz, float end_hz, float seconds, float attack_seconds,
                                float release_seconds, float tone_gain, float noise_gain,
                                float thump_gain, uint32_t seed) {
  const unsigned int sample_rate = 32000u;
  const unsigned int frame_count = (unsigned int)(seconds * (float)sample_rate);
  int16_t* samples = malloc(frame_count * sizeof(int16_t));
  Wave wave = {0};
  Sound sound = {0};
  uint32_t noise_state = seed != 0u ? seed : 1u;
  float previous_sample = 0.0f;
  float previous_noise = 0.0f;

  if ((samples == NULL) || (frame_count == 0u)) {
    free(samples);
    return sound;
  }

  for (unsigned int index = 0u; index < frame_count; ++index) {
    const float t = (float)index / (float)sample_rate;
    const float progress = (float)index / (float)frame_count;
    const float hz = start_hz + ((end_hz - start_hz) * SmoothStep01(progress));
    const float attack = SmoothStep01(t / fmaxf(attack_seconds, 0.001f));
    const float release = SmoothStep01((seconds - t) / fmaxf(release_seconds, 0.001f));
    const float envelope = attack * release;
    const float noise = (previous_noise * 0.82f) + (NextNoiseSample(&noise_state) * 0.18f);
    const float fundamental = sinf(2.0f * PI * hz * t);
    const float overtone = sinf(2.0f * PI * hz * 2.0f * t) * 0.12f;
    const float warm_body =
        sinf(2.0f * PI * fmaxf(42.0f, start_hz * 0.42f) * t) * expf(-progress * 8.0f) * thump_gain;
    const float wobble = 0.92f + (0.08f * sinf(2.0f * PI * 5.0f * t));
    const float raw_sample =
        ((fundamental + overtone) * tone_gain * wobble + (noise * noise_gain) + warm_body) *
        envelope;
    const float filtered_sample = (previous_sample * 0.58f) + (tanhf(raw_sample) * 0.42f);
    previous_noise = noise;
    previous_sample = filtered_sample;
    samples[index] = (int16_t)(Clamp(filtered_sample, -1.0f, 1.0f) * 11500.0f);
  }

  wave.frameCount = frame_count;
  wave.sampleRate = sample_rate;
  wave.sampleSize = 16u;
  wave.channels = 1u;
  wave.data = samples;
  sound = LoadSoundFromWave(wave);
  free(samples);
  return sound;
}

static Sound GenerateAmbientLoopSound(void) {
  const unsigned int sample_rate = 32000u;
  const float seconds = 3.2f;
  const unsigned int frame_count = (unsigned int)(seconds * (float)sample_rate);
  int16_t* samples = malloc(frame_count * sizeof(int16_t));
  Wave wave = {0};
  Sound sound = {0};

  if (samples == NULL) {
    free(samples);
    return sound;
  }

  for (unsigned int index = 0u; index < frame_count; ++index) {
    const float t = (float)index / (float)sample_rate;
    const float loop_phase = (float)index / (float)frame_count;
    const float fade = sinf(PI * loop_phase);
    const float drone = sinf(2.0f * PI * 92.0f * t) * 0.34f;
    const float overtone = sinf(2.0f * PI * 184.0f * t + sinf(2.0f * PI * 0.31f * t)) * 0.18f;
    const float shimmer = sinf(2.0f * PI * 276.0f * t) * sinf(2.0f * PI * 0.17f * t) * 0.08f;
    const float sample = (drone + overtone + shimmer) * (0.52f + (0.48f * fade));
    samples[index] = (int16_t)(Clamp(sample, -1.0f, 1.0f) * 5200.0f);
  }

  wave.frameCount = frame_count;
  wave.sampleRate = sample_rate;
  wave.sampleSize = 16u;
  wave.channels = 1u;
  wave.data = samples;
  sound = LoadSoundFromWave(wave);
  free(samples);
  return sound;
}

static void EnsureClientSfxLoaded(ShroomClientSfx sfx) {
  if ((sfx < 0) || (sfx >= SHROOM_CLIENT_SFX_COUNT) || g_client_sfx_loaded[sfx] ||
      !IsAudioDeviceReady()) {
    return;
  }

  switch (sfx) {
  case SHROOM_CLIENT_SFX_SPORE:
    g_client_sfx[sfx] =
        GenerateOrganicSfx(560.0f, 940.0f, 0.13f, 0.006f, 0.09f, 0.36f, 0.035f, 0.02f, 0x51A1u);
    break;
  case SHROOM_CLIENT_SFX_CONSUME:
    g_client_sfx[sfx] =
        GenerateOrganicSfx(190.0f, 86.0f, 0.36f, 0.008f, 0.28f, 0.30f, 0.055f, 0.34f, 0xC08Eu);
    break;
  case SHROOM_CLIENT_SFX_DEATH:
    g_client_sfx[sfx] =
        GenerateOrganicSfx(210.0f, 54.0f, 0.48f, 0.012f, 0.38f, 0.26f, 0.050f, 0.30f, 0xDEADu);
    break;
  case SHROOM_CLIENT_SFX_ZONE:
    g_client_sfx[sfx] =
        GenerateOrganicSfx(300.0f, 560.0f, 0.34f, 0.020f, 0.24f, 0.28f, 0.030f, 0.04f, 0x20A1u);
    break;
  case SHROOM_CLIENT_SFX_WARNING:
    g_client_sfx[sfx] =
        GenerateOrganicSfx(360.0f, 220.0f, 0.24f, 0.006f, 0.17f, 0.30f, 0.025f, 0.10f, 0xA11Eu);
    break;
  case SHROOM_CLIENT_SFX_POWERUP:
    g_client_sfx[sfx] =
        GenerateOrganicSfx(440.0f, 880.0f, 0.24f, 0.008f, 0.16f, 0.34f, 0.020f, 0.05f, 0xB005u);
    break;
  case SHROOM_CLIENT_SFX_SPLIT:
    g_client_sfx[sfx] =
        GenerateOrganicSfx(160.0f, 360.0f, 0.26f, 0.004f, 0.20f, 0.24f, 0.075f, 0.18f, 0x5117u);
    break;
  case SHROOM_CLIENT_SFX_UI_CLICK:
    g_client_sfx[sfx] =
        GenerateOrganicSfx(620.0f, 720.0f, 0.07f, 0.002f, 0.045f, 0.22f, 0.010f, 0.00f, 0xC11Cu);
    break;
  case SHROOM_CLIENT_SFX_UI_ERROR:
    g_client_sfx[sfx] =
        GenerateOrganicSfx(180.0f, 120.0f, 0.18f, 0.004f, 0.13f, 0.28f, 0.030f, 0.12f, 0xE220u);
    break;
  case SHROOM_CLIENT_SFX_COUNT:
  default:
    return;
  }
  g_client_sfx_loaded[sfx] = true;
}

void ShroomClientAudioEnsureAllSfxLoaded(void) {
  for (int index = 0; index < SHROOM_CLIENT_SFX_COUNT; ++index) {
    EnsureClientSfxLoaded((ShroomClientSfx)index);
  }
}

static void EnsureClientMusicLoaded(void) {
  if (g_client_music_loaded || !IsAudioDeviceReady()) {
    return;
  }
  g_client_music_loop = GenerateAmbientLoopSound();
  g_client_music_loaded = true;
}

void ShroomClientAudioUpdateMusic(const ClientSettings* settings) {
  float volume;

  if (settings == NULL) {
    return;
  }
  EnsureClientMusicLoaded();
  if (!g_client_music_loaded) {
    return;
  }

  volume = ((float)settings->music_volume_percent / 100.0f) * 0.34f;
  SetSoundVolume(g_client_music_loop, volume);
  SetSoundPan(g_client_music_loop, 0.5f);
  if ((volume > 0.0f) && !IsSoundPlaying(g_client_music_loop)) {
    PlaySound(g_client_music_loop);
  }
  if ((volume <= 0.0f) && IsSoundPlaying(g_client_music_loop)) {
    StopSound(g_client_music_loop);
  }
}

void ShroomClientAudioShutdown(void) {
  if (!IsAudioDeviceReady()) {
    return;
  }
  for (int index = 0; index < SHROOM_CLIENT_SFX_COUNT; ++index) {
    if (g_client_sfx_loaded[index]) {
      UnloadSound(g_client_sfx[index]);
    }
  }
  memset(g_client_sfx, 0, sizeof(g_client_sfx));
  memset(g_client_sfx_loaded, 0, sizeof(g_client_sfx_loaded));
  if (g_client_music_loaded) {
    UnloadSound(g_client_music_loop);
  }
  memset(&g_client_music_loop, 0, sizeof(g_client_music_loop));
  g_client_music_loaded = false;
}

void ShroomClientAudioPlaySfx(const ClientSettings* settings, ShroomClientSfx sfx,
                              float importance) {
  float volume;
  int active_sounds = 0;

  if ((settings == NULL) || (sfx < 0) || (sfx >= SHROOM_CLIENT_SFX_COUNT)) {
    return;
  }

  EnsureClientSfxLoaded(sfx);
  if (!g_client_sfx_loaded[sfx]) {
    return;
  }
  if (IsSoundPlaying(g_client_sfx[sfx]) &&
      (sfx == SHROOM_CLIENT_SFX_SPORE || sfx == SHROOM_CLIENT_SFX_WARNING)) {
    return;
  }

  if ((sfx == SHROOM_CLIENT_SFX_DEATH) || (sfx == SHROOM_CLIENT_SFX_CONSUME)) {
    for (int index = 0; index < SHROOM_CLIENT_SFX_COUNT; ++index) {
      if ((index != (int)sfx) && g_client_sfx_loaded[index] &&
          IsSoundPlaying(g_client_sfx[index])) {
        StopSound(g_client_sfx[index]);
      }
    }
  }

  for (int index = 0; index < SHROOM_CLIENT_SFX_COUNT; ++index) {
    if (g_client_sfx_loaded[index] && IsSoundPlaying(g_client_sfx[index])) {
      active_sounds += 1;
    }
  }

  volume = ((float)settings->effects_volume_percent / 100.0f) * Clamp(importance, 0.0f, 1.0f);
  volume *= 0.84f / (1.0f + ((float)active_sounds * 0.24f));
  SetSoundVolume(g_client_sfx[sfx], volume);
  SetSoundPan(g_client_sfx[sfx], 0.5f);
  PlaySound(g_client_sfx[sfx]);
}
