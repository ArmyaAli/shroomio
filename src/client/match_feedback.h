#ifndef SHROOM_CLIENT_MATCH_FEEDBACK_H
#define SHROOM_CLIENT_MATCH_FEEDBACK_H

#include <stdbool.h>

#include "shared/world.h"

bool ShroomMatchFeedbackNeedsRebaseline(ShroomMatchPhase previous_phase,
                                        ShroomMatchPhase current_phase);

#endif
