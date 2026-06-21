#ifndef SHROOM_CLIENT_AUDIO_H
#define SHROOM_CLIENT_AUDIO_H

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

void ShroomClientAudioEnsureAllSfxLoaded(void);
void ShroomClientAudioUpdateMusic(const ClientSettings* settings);
void ShroomClientAudioShutdown(void);
void ShroomClientAudioPlaySfx(const ClientSettings* settings, ShroomClientSfx sfx,
                              float importance);

#endif
