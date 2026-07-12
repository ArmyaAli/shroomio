#ifndef SHROOM_SIM_H
#define SHROOM_SIM_H

#include "world.h"

void ShroomWorldInit(ShroomWorldState* world);
void ShroomWorldInitWithSeed(ShroomWorldState* world, uint32_t seed);
void ShroomWorldStep(ShroomWorldState* world, float delta_time);
void ShroomWorldSetMatchDuration(ShroomWorldState* world, float duration_seconds);
void ShroomWorldResetMatch(ShroomWorldState* world);
void ShroomComputeMatchPodium(ShroomWorldState* world);
float ShroomWorldGetColonyMass(const ShroomWorldState* world, ShroomPlayerId player_id);

ShroomPlayerState* ShroomWorldSpawnPlayer(ShroomWorldState* world, ShroomPlayerId player_id,
                                          bool is_bot);

void ShroomPlayerSetInput(ShroomPlayerState* player, ShroomVec2 input_direction);
bool ShroomPlayerHasConsumeProtection(const ShroomPlayerState* player);
bool ShroomPlayerCanConsume(const ShroomWorldState* world, const ShroomPlayerState* attacker,
                            const ShroomPlayerState* target);
bool ShroomPlayerCanDecay(const ShroomWorldState* world, const ShroomPlayerState* player);
bool ShroomPlayerCanSplit(const ShroomWorldState* world, const ShroomPlayerState* player);
bool ShroomPlayersCanMerge(const ShroomPlayerState* primary, const ShroomPlayerState* piece);
bool ShroomWorldSplitPlayer(ShroomWorldState* world, ShroomPlayerState* player);
bool ShroomWorldSplitPlayerToward(ShroomWorldState* world, ShroomPlayerState* player,
                                  ShroomVec2 aim_direction);
bool ShroomWorldEjectMass(ShroomWorldState* world, ShroomPlayerState* player,
                          ShroomVec2 aim_direction);

float ShroomMassToRadius(float mass);
float ShroomMassToSpeed(float mass);
ShroomZone ShroomGetZoneAtPosition(const ShroomWorldState* world, ShroomVec2 position);
float ShroomGetConsumeMassAdvantageAtPosition(const ShroomWorldState* world, ShroomVec2 position);
float ShroomDistanceSqr(ShroomVec2 a, ShroomVec2 b);

#endif
