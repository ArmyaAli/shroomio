#ifndef SHROOMIO_CLIENT_RESULTS_SUMMARY_H
#define SHROOMIO_CLIENT_RESULTS_SUMMARY_H

#include <stddef.h>
#include <stdint.h>

uint32_t ShroomResultsElapsedSeconds(float start_time, float end_time);
void ShroomResultsFormatDuration(uint32_t duration_seconds, char* buffer, size_t buffer_size);

#endif
