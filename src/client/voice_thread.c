#include "voice_thread.h"

#include <stdlib.h>

typedef struct ShroomVoiceThreadArgs {
  ShroomVoiceThreadFn function;
  void* context;
} ShroomVoiceThreadArgs;

#if defined(_WIN32)
#include <process.h>
#include <windows.h>

static unsigned __stdcall RunThread(void* context) {
  ShroomVoiceThreadArgs* start = context;
  const ShroomVoiceThreadFn function = start->function;
  void* function_context = start->context;

  free(start);
  return (unsigned)function(function_context);
}

bool ShroomVoiceThreadStart(ShroomVoiceThread* thread, ShroomVoiceThreadFn function,
                            void* context) {
  ShroomVoiceThreadArgs* start;
  uintptr_t handle;

  if ((thread == NULL) || (function == NULL) || (thread->handle != NULL)) {
    return false;
  }
  start = malloc(sizeof(*start));
  if (start == NULL) {
    return false;
  }
  *start = (ShroomVoiceThreadArgs){.function = function, .context = context};
  handle = _beginthreadex(NULL, 0u, RunThread, start, 0u, NULL);
  if (handle == 0u) {
    free(start);
    return false;
  }
  thread->handle = (void*)handle;
  return true;
}

void ShroomVoiceThreadJoin(ShroomVoiceThread* thread) {
  if ((thread != NULL) && (thread->handle != NULL)) {
    WaitForSingleObject((HANDLE)thread->handle, INFINITE);
    CloseHandle((HANDLE)thread->handle);
    thread->handle = NULL;
  }
}

void ShroomVoiceThreadSleepMs(uint32_t milliseconds) { Sleep(milliseconds); }

#else
#include <pthread.h>
#include <time.h>

typedef struct ShroomVoicePosixThread {
  pthread_t id;
} ShroomVoicePosixThread;

static void* RunThread(void* context) {
  ShroomVoiceThreadArgs* start = context;
  const ShroomVoiceThreadFn function = start->function;
  void* function_context = start->context;

  free(start);
  (void)function(function_context);
  return NULL;
}

bool ShroomVoiceThreadStart(ShroomVoiceThread* thread, ShroomVoiceThreadFn function,
                            void* context) {
  ShroomVoicePosixThread* handle;
  ShroomVoiceThreadArgs* start;

  if ((thread == NULL) || (function == NULL) || (thread->handle != NULL)) {
    return false;
  }
  handle = malloc(sizeof(*handle));
  start = malloc(sizeof(*start));
  if ((handle == NULL) || (start == NULL)) {
    free(handle);
    free(start);
    return false;
  }
  *start = (ShroomVoiceThreadArgs){.function = function, .context = context};
  if (pthread_create(&handle->id, NULL, RunThread, start) != 0) {
    free(start);
    free(handle);
    return false;
  }
  thread->handle = handle;
  return true;
}

void ShroomVoiceThreadJoin(ShroomVoiceThread* thread) {
  if ((thread != NULL) && (thread->handle != NULL)) {
    ShroomVoicePosixThread* handle = thread->handle;
    (void)pthread_join(handle->id, NULL);
    free(handle);
    thread->handle = NULL;
  }
}

void ShroomVoiceThreadSleepMs(uint32_t milliseconds) {
  struct timespec duration = {
      .tv_sec = (time_t)(milliseconds / 1000u),
      .tv_nsec = (long)(milliseconds % 1000u) * 1000000L,
  };
  while (nanosleep(&duration, &duration) != 0) {
  }
}
#endif
