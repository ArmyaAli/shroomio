#ifndef SHROOM_CLIENT_VOICE_THREAD_H
#define SHROOM_CLIENT_VOICE_THREAD_H

#include <stdbool.h>
#include <stdint.h>

typedef int (*ShroomVoiceThreadFn)(void* context);

typedef struct ShroomVoiceThread {
  void* handle;
} ShroomVoiceThread;

bool ShroomVoiceThreadStart(ShroomVoiceThread* thread, ShroomVoiceThreadFn function, void* context);
void ShroomVoiceThreadJoin(ShroomVoiceThread* thread);
void ShroomVoiceThreadSleepMs(uint32_t milliseconds);

#endif
