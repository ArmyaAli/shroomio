#include "input_admission.h"

#include <stddef.h>

#define SHROOM_INPUT_TOKEN_MILLI 1000u
#define SHROOM_INPUT_REFILL_MILLI_PER_MS SHROOM_SERVER_INPUT_RATE_HZ
#define SHROOM_INPUT_CAPACITY_MILLI (SHROOM_SERVER_INPUT_BURST_CAPACITY * SHROOM_INPUT_TOKEN_MILLI)

static bool SequenceIsNewer(uint32_t sequence, uint32_t previous) {
  const uint32_t delta = sequence - previous;
  return (delta != 0u) && (delta < 0x80000000u);
}

static void Refill(ShroomInputAdmission* admission, uint64_t now_ms) {
  uint64_t elapsed_ms;
  uint64_t refill;

  if (!admission->initialized) {
    admission->initialized = true;
    admission->last_refill_ms = now_ms;
    admission->token_milli = SHROOM_INPUT_CAPACITY_MILLI;
    return;
  }
  if (now_ms < admission->last_refill_ms) {
    admission->last_refill_ms = now_ms;
    return;
  }
  elapsed_ms = now_ms - admission->last_refill_ms;
  admission->last_refill_ms = now_ms;
  refill = elapsed_ms * SHROOM_INPUT_REFILL_MILLI_PER_MS;
  if ((refill >= SHROOM_INPUT_CAPACITY_MILLI) ||
      (admission->token_milli >= SHROOM_INPUT_CAPACITY_MILLI - refill)) {
    admission->token_milli = SHROOM_INPUT_CAPACITY_MILLI;
  } else {
    admission->token_milli += (uint32_t)refill;
  }
}

void ShroomInputAdmissionReset(ShroomInputAdmission* admission) {
  if (admission != NULL) {
    *admission = (ShroomInputAdmission){0};
  }
}

ShroomInputAdmissionResult ShroomInputAdmissionCheck(ShroomInputAdmission* admission,
                                                     uint32_t sequence, uint64_t now_ms) {
  if (admission == NULL) {
    return SHROOM_INPUT_REJECTED_RATE;
  }
  Refill(admission, now_ms);
  if (admission->token_milli < SHROOM_INPUT_TOKEN_MILLI) {
    admission->rate_limited_count += 1u;
    return SHROOM_INPUT_REJECTED_RATE;
  }
  admission->token_milli -= SHROOM_INPUT_TOKEN_MILLI;

  if ((sequence == 0u) ||
      (admission->has_sequence && !SequenceIsNewer(sequence, admission->last_sequence))) {
    admission->stale_count += 1u;
    return SHROOM_INPUT_REJECTED_STALE;
  }
  admission->has_sequence = true;
  admission->last_sequence = sequence;
  admission->accepted_count += 1u;
  return SHROOM_INPUT_ADMITTED;
}
