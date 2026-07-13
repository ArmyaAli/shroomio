#ifndef SHROOMIO_CLIENT_RESULTS_SUMMARY_H
#define SHROOMIO_CLIENT_RESULTS_SUMMARY_H

#include <stddef.h>
#include <stdint.h>

#include "shared/intermission.h"

uint32_t ShroomResultsElapsedSeconds(float start_time, float end_time);
void ShroomResultsFormatDuration(uint32_t duration_seconds, char* buffer, size_t buffer_size);
const char* ShroomResultsVoteLabel(ShroomRematchVote vote);
void ShroomResultsFormatVoteParticipation(uint16_t play_again_votes, uint16_t return_to_lobby_votes,
                                          uint16_t spectate_votes, uint16_t eligible_count,
                                          char* buffer, size_t buffer_size);
void ShroomResultsFormatLocalVote(ShroomRematchVote vote, bool can_vote, char* buffer,
                                  size_t buffer_size);
void ShroomResultsFormatDecision(ShroomRematchVote decision, char* buffer, size_t buffer_size);

#endif
