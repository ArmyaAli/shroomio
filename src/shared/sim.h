#ifndef SHROOM_SIM_H
#define SHROOM_SIM_H

#include "world.h"

void ShroomWorldInit(ShroomWorldState* world);
void ShroomWorldInitWithSeed(ShroomWorldState* world, uint32_t seed);
void ShroomWorldStep(ShroomWorldState* world, float delta_time);

ShroomPlayerState* ShroomWorldSpawnPlayer(ShroomWorldState* world, ShroomPlayerId player_id,
                                          bool is_bot);

void ShroomPlayerSetInput(ShroomPlayerState* player, ShroomVec2 input_direction);

float ShroomMassToRadius(float mass);
float ShroomMassToSpeed(float mass);
ShroomZone ShroomGetZoneAtPosition(const ShroomWorldState* world, ShroomVec2 position);
float ShroomDistanceSqr(ShroomVec2 a, ShroomVec2 b);

#endif
