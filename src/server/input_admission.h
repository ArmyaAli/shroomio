#ifndef SHROOM_SERVER_INPUT_ADMISSION_H
#define SHROOM_SERVER_INPUT_ADMISSION_H

#include <stdbool.h>
#include <stdint.h>

#define SHROOM_SERVER_INPUT_RATE_HZ 30u
#define SHROOM_SERVER_INPUT_BURST_CAPACITY 10u
#define SHROOM_SERVER_MAX_ENET_EVENTS_PER_TICK 4096u

typedef enum ShroomInputAdmissionResult {
  SHROOM_INPUT_ADMITTED = 0,
  SHROOM_INPUT_REJECTED_STALE,
  SHROOM_INPUT_REJECTED_RATE,
} ShroomInputAdmissionResult;

typedef struct ShroomInputAdmission {
  bool initialized;
  bool has_sequence;
  uint32_t last_sequence;
  uint64_t last_refill_ms;
  uint32_t token_milli;
  uint64_t accepted_count;
  uint64_t stale_count;
  uint64_t rate_limited_count;
} ShroomInputAdmission;

void ShroomInputAdmissionReset(ShroomInputAdmission* admission);
ShroomInputAdmissionResult ShroomInputAdmissionCheck(ShroomInputAdmission* admission,
                                                     uint32_t sequence, uint64_t now_ms);

#endif
