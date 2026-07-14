#ifndef SHROOM_CLIENT_VOICE_MIXER_H
#define SHROOM_CLIENT_VOICE_MIXER_H

#include <stddef.h>

void ShroomVoiceMixAdd(float* mix, const float* source, size_t sample_count, float volume);
void ShroomVoiceMixClamp(float* mix, size_t sample_count);

#endif
