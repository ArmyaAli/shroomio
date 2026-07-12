#ifndef SHROOM_CLIENT_AUDIO_H
#define SHROOM_CLIENT_AUDIO_H

#include <stdbool.h>

#include "client_settings.h"

typedef enum ShroomClientSfx {
  SHROOM_CLIENT_SFX_SPORE = 0,
  SHROOM_CLIENT_SFX_CONSUME,
  SHROOM_CLIENT_SFX_DEATH,
  SHROOM_CLIENT_SFX_ZONE,
  SHROOM_CLIENT_SFX_WARNING,
  SHROOM_CLIENT_SFX_POWERUP,
  SHROOM_CLIENT_SFX_SPLIT,
  SHROOM_CLIENT_SFX_UI_CLICK,
  SHROOM_CLIENT_SFX_UI_ERROR,
  SHROOM_CLIENT_SFX_COUNT,
} ShroomClientSfx;

bool ShroomClientAudioInit(const ClientSettings* settings);
bool ShroomClientAudioRestart(const ClientSettings* settings);
bool ShroomClientAudioIsReady(void);
const char* ShroomClientAudioGetStatus(void);
void ShroomClientAudioUpdateMusic(const ClientSettings* settings);
void ShroomClientAudioShutdown(void);
void ShroomClientAudioPlaySfx(const ClientSettings* settings, ShroomClientSfx sfx,
                              float importance);

#ifdef TEST_MODE
typedef struct ShroomClientAudioTestBackend {
  void* context;
  bool (*init_device)(void* context);
  bool (*device_ready)(void* context);
  void (*close_device)(void* context);
  bool (*load_assets)(void* context);
  void (*unload_assets)(void* context);
  void (*apply_settings)(void* context, const ClientSettings* settings);
  void (*update_music)(void* context, const ClientSettings* settings);
} ShroomClientAudioTestBackend;

void ShroomClientAudioTestSetBackend(const ShroomClientAudioTestBackend* backend);
bool ShroomClientAudioTestAssetsLoaded(void);
void ShroomClientAudioTestResetThrottleState(void);
bool ShroomClientAudioTestCanPlaySfx(ShroomClientSfx sfx, double now_seconds);
#endif

#endif
