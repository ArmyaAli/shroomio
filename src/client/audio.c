#include "audio.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "raymath.h"

static Sound g_client_sfx[SHROOM_CLIENT_SFX_COUNT];
static bool g_client_sfx_loaded[SHROOM_CLIENT_SFX_COUNT];
static double g_client_sfx_last_played_at[SHROOM_CLIENT_SFX_COUNT];
static Sound g_client_music_loop;
static bool g_client_music_loaded;

static bool SoundIsUsable(Sound sound) {
  return (sound.stream.buffer != NULL) && (sound.frameCount > 0u);
}

static float SmoothStep01(float value) {
  const float t = Clamp(value, 0.0f, 1.0f);
  return t * t * (3.0f - (2.0f * t));
}

static float NextNoiseSample(uint32_t* state) {
  *state = (*state * 1664525u) + 1013904223u;
  return ((float)((*state >> 8u) & 0xFFFFu) / 32767.5f) - 1.0f;
}

static double GetSfxCooldownSeconds(ShroomClientSfx sfx) {
  switch (sfx) {
  case SHROOM_CLIENT_SFX_SPORE:
    return 0.16;
  case SHROOM_CLIENT_SFX_CONSUME:
    return 0.14;
  case SHROOM_CLIENT_SFX_WARNING:
    return 0.75;
  case SHROOM_CLIENT_SFX_POWERUP:
  case SHROOM_CLIENT_SFX_SPLIT:
    return 0.12;
  case SHROOM_CLIENT_SFX_UI_CLICK:
  case SHROOM_CLIENT_SFX_UI_ERROR:
    return 0.04;
  case SHROOM_CLIENT_SFX_DEATH:
  case SHROOM_CLIENT_SFX_COUNT:
  default:
    return 0.0;
  }
}

static bool CanPlaySfxNow(ShroomClientSfx sfx, double now_seconds) {
  const double cooldown = GetSfxCooldownSeconds(sfx);

  if ((sfx < 0) || (sfx >= SHROOM_CLIENT_SFX_COUNT)) {
    return false;
  }
  if (cooldown <= 0.0) {
    g_client_sfx_last_played_at[sfx] = now_seconds;
    return true;
  }
  if ((g_client_sfx_last_played_at[sfx] > 0.0) &&
      ((now_seconds - g_client_sfx_last_played_at[sfx]) < cooldown)) {
    return false;
  }

  g_client_sfx_last_played_at[sfx] = now_seconds;
  return true;
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

static float NoteToHz(int note) {
  // Convert MIDI note number to frequency (A4 = 440Hz = MIDI note 69)
  return 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
}

static Sound GenerateAmbientLoopSound(void) {
  const unsigned int sample_rate = 32000u;
  const float seconds = 48.0f; // 48 seconds for a longer, more immersive loop
  const unsigned int frame_count = (unsigned int)(seconds * (float)sample_rate);
  int16_t* samples = malloc(frame_count * sizeof(int16_t));
  Wave wave = {0};
  Sound sound = {0};

  if (samples == NULL) {
    free(samples);
    return sound;
  }

  // Peaceful chord progression in C major: C - G - Am - F
  // Each chord lasts 4 beats at 60 BPM = 4 seconds per chord
  // Total progression = 16 seconds, loops 3 times in 48 seconds
  const int chord_progression[] = {60, 67, 69, 65}; // C4, G4, A4, F4 (root notes)
  const float chord_duration = 4.0f;                // seconds per chord
  const float bpm = 60.0f;
  const float beat_duration = 60.0f / bpm;

  for (unsigned int index = 0u; index < frame_count; ++index) {
    const float t = (float)index / (float)sample_rate;

    // Smooth fade in/out for seamless looping
    const float fade_in = SmoothStep01(t / 2.0f);
    const float fade_out = SmoothStep01((seconds - t) / 2.0f);
    const float loop_fade = fade_in * fade_out;

    // Determine current chord
    const float progression_time = fmodf(t, 16.0f); // 16-second progression
    const int chord_index = (int)(progression_time / chord_duration) % 4;
    const int chord_root = chord_progression[chord_index];
    const float chord_time = fmodf(progression_time, chord_duration);
    const float chord_fade = sinf(PI * (chord_time / chord_duration)); // Smooth chord transitions

    // Bass drone (root note, one octave down)
    const float bass_hz = NoteToHz(chord_root - 12);
    const float bass = sinf(2.0f * PI * bass_hz * t) * 0.25f;

    // Chord pad (root, third, fifth)
    const float root_hz = NoteToHz(chord_root);
    const float third_hz = NoteToHz(chord_root + 4); // Major third
    const float fifth_hz = NoteToHz(chord_root + 7); // Perfect fifth
    const float pad = (sinf(2.0f * PI * root_hz * t) + sinf(2.0f * PI * third_hz * t) * 0.7f +
                       sinf(2.0f * PI * fifth_hz * t) * 0.5f) *
                      0.15f;

    // Gentle melody notes (arpeggiate through chord tones)
    const float melody_time = fmodf(t, beat_duration * 2.0f);
    const int melody_note_index = (int)(melody_time / beat_duration) % 2;
    const int melody_offset = melody_note_index == 0 ? 0 : 7;          // Alternate root and fifth
    const float melody_hz = NoteToHz(chord_root + 12 + melody_offset); // One octave up
    const float melody_envelope = sinf(PI * (melody_time / (beat_duration * 2.0f)));
    const float melody = sinf(2.0f * PI * melody_hz * t) * 0.08f * melody_envelope;

    // Subtle shimmer/high harmonics
    const float shimmer_hz = NoteToHz(chord_root + 24); // Two octaves up
    const float shimmer = sinf(2.0f * PI * shimmer_hz * t + sinf(2.0f * PI * 0.2f * t)) * 0.03f;

    // Combine all elements
    const float sample = (bass + pad * chord_fade + melody + shimmer) * loop_fade;

    // Soft clipping to avoid harsh artifacts
    const float soft_sample = tanhf(sample * 1.2f) * 0.85f;

    samples[index] = (int16_t)(Clamp(soft_sample, -1.0f, 1.0f) * 6000.0f);
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
  g_client_sfx_loaded[sfx] = SoundIsUsable(g_client_sfx[sfx]);
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
  g_client_music_loaded = SoundIsUsable(g_client_music_loop);
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
  memset(g_client_sfx_last_played_at, 0, sizeof(g_client_sfx_last_played_at));
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
  if (!CanPlaySfxNow(sfx, GetTime())) {
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

#ifdef TEST_MODE
void ShroomClientAudioTestResetThrottleState(void) {
  memset(g_client_sfx_last_played_at, 0, sizeof(g_client_sfx_last_played_at));
}

bool ShroomClientAudioTestCanPlaySfx(ShroomClientSfx sfx, double now_seconds) {
  return CanPlaySfxNow(sfx, now_seconds);
}
#endif
