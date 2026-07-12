#include "match_feedback.h"

bool ShroomMatchFeedbackNeedsRebaseline(ShroomMatchPhase previous_phase,
                                        ShroomMatchPhase current_phase) {
  return (previous_phase == SHROOM_MATCH_PHASE_RESULTS) &&
         (current_phase == SHROOM_MATCH_PHASE_RUNNING);
}
