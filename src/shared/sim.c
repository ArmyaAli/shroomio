#include "sim.h"

#include <stdlib.h>

static float ShroomClamp(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }

    return value;
}

static float ShroomRandomFloat(float min_value, float max_value) {
    return min_value + ((float)rand() / (float)RAND_MAX) * (max_value - min_value);
}

float ShroomDistanceSqr(ShroomVec2 a, ShroomVec2 b) {
    const ShroomVec2 delta = ShroomVec2Sub(a, b);
    return ShroomVec2LengthSqr(delta);
}

static ShroomVec2 ShroomNormalizeOrZero(ShroomVec2 v) {
    const float length_sqr = ShroomVec2LengthSqr(v);
    float scale;

    if (length_sqr <= 0.0001f) {
        return (ShroomVec2){0};
    }

    scale = 1.0f / __builtin_sqrtf(length_sqr);
    return ShroomVec2Scale(v, scale);
}

static ShroomVec2 ShroomWorldCenter(const ShroomWorldState *world) {
    return (ShroomVec2){world->width * 0.5f, world->height * 0.5f};
}

ShroomZone ShroomGetZoneAtPosition(const ShroomWorldState *world, ShroomVec2 position) {
    const float distance_sqr = ShroomDistanceSqr(position, ShroomWorldCenter(world));

    if (distance_sqr <= (SHROOM_ZONE_CENTER_RADIUS * SHROOM_ZONE_CENTER_RADIUS)) {
        return SHROOM_ZONE_CENTER;
    }
    if (distance_sqr <= (SHROOM_ZONE_MID_RADIUS * SHROOM_ZONE_MID_RADIUS)) {
        return SHROOM_ZONE_MID;
    }

    return SHROOM_ZONE_OUTER;
}

static bool ShroomIsSafeSpawn(const ShroomWorldState *world, ShroomVec2 position) {
    size_t index;

    for (index = 0; index < world->player_count; ++index) {
        const ShroomPlayerState *other = &world->players[index];
        const float safety_radius = other->radius + SHROOM_SPAWN_SAFE_DISTANCE;

        if (!other->alive) {
            continue;
        }

        if (ShroomDistanceSqr(position, other->position) < (safety_radius * safety_radius)) {
            return false;
        }
    }

    return true;
}

static ShroomVec2 ShroomRandomSpawnPosition(const ShroomWorldState *world, bool prefer_outer) {
    const float padding = 120.0f;
    size_t attempt;

    for (attempt = 0; attempt < 32; ++attempt) {
        const ShroomVec2 candidate = {
            ShroomRandomFloat(padding, world->width - padding),
            ShroomRandomFloat(padding, world->height - padding),
        };
        const ShroomZone zone = ShroomGetZoneAtPosition(world, candidate);

        if (prefer_outer && zone != SHROOM_ZONE_OUTER) {
            continue;
        }
        if (ShroomIsSafeSpawn(world, candidate)) {
            return candidate;
        }
    }

    return (ShroomVec2){ShroomRandomFloat(padding, world->width - padding), ShroomRandomFloat(padding, world->height - padding)};
}

static void ShroomRespawnPlayer(const ShroomWorldState *world, ShroomPlayerState *player) {
    player->position = ShroomRandomSpawnPosition(world, true);
    player->input_direction = (ShroomVec2){0};
    player->mass = SHROOM_DEFAULT_PLAYER_MASS;
    player->radius = ShroomMassToRadius(player->mass);
    player->alive = true;
}

static void ShroomSpawnOrResetSpore(ShroomWorldState *world, ShroomSporeState *spore) {
    float min_x = 60.0f;
    float min_y = 60.0f;
    float max_x = world->width - 60.0f;
    float max_y = world->height - 60.0f;
    float center_bias = ShroomRandomFloat(0.0f, 1.0f);
    ShroomVec2 candidate;

    if (center_bias < 0.45f) {
        const ShroomVec2 center = ShroomWorldCenter(world);
        candidate.x = ShroomRandomFloat(center.x - SHROOM_ZONE_CENTER_RADIUS, center.x + SHROOM_ZONE_CENTER_RADIUS);
        candidate.y = ShroomRandomFloat(center.y - SHROOM_ZONE_CENTER_RADIUS, center.y + SHROOM_ZONE_CENTER_RADIUS);
    } else if (center_bias < 0.80f) {
        const ShroomVec2 center = ShroomWorldCenter(world);
        candidate.x = ShroomRandomFloat(center.x - SHROOM_ZONE_MID_RADIUS, center.x + SHROOM_ZONE_MID_RADIUS);
        candidate.y = ShroomRandomFloat(center.y - SHROOM_ZONE_MID_RADIUS, center.y + SHROOM_ZONE_MID_RADIUS);
    } else {
        candidate.x = ShroomRandomFloat(min_x, max_x);
        candidate.y = ShroomRandomFloat(min_y, max_y);
    }

    candidate.x = ShroomClamp(candidate.x, min_x, max_x);
    candidate.y = ShroomClamp(candidate.y, min_y, max_y);

    spore->position = candidate;
    spore->value = SHROOM_SPORE_VALUE;
    spore->active = true;
    if (spore->entity_id == 0) {
        spore->entity_id = world->next_entity_id++;
    }
}

static void ShroomInitializeSpores(ShroomWorldState *world) {
    size_t index;

    world->spore_count = SHROOM_SPORE_TARGET_COUNT;
    for (index = 0; index < world->spore_count; ++index) {
        ShroomSpawnOrResetSpore(world, &world->spores[index]);
    }
}

static void ShroomUpdateBotInput(ShroomWorldState *world, ShroomPlayerState *bot) {
    size_t index;
    const ShroomPlayerState *best_threat = 0;
    const ShroomPlayerState *best_prey = 0;
    const ShroomSporeState *best_spore = 0;
    float best_threat_distance = 0.0f;
    float best_prey_distance = 0.0f;
    float best_spore_distance = 0.0f;

    for (index = 0; index < world->player_count; ++index) {
        const ShroomPlayerState *other = &world->players[index];
        const float distance_sqr = ShroomDistanceSqr(bot->position, other->position);

        if (other == bot || !other->alive) {
            continue;
        }

        if (other->mass > (bot->mass * SHROOM_CONSUME_MASS_ADVANTAGE)) {
            if ((best_threat == 0) || (distance_sqr < best_threat_distance)) {
                best_threat = other;
                best_threat_distance = distance_sqr;
            }
        } else if (bot->mass > (other->mass * SHROOM_CONSUME_MASS_ADVANTAGE)) {
            if ((best_prey == 0) || (distance_sqr < best_prey_distance)) {
                best_prey = other;
                best_prey_distance = distance_sqr;
            }
        }
    }

    if ((best_threat != 0) && (best_threat_distance < (900.0f * 900.0f))) {
        bot->input_direction = ShroomNormalizeOrZero(ShroomVec2Sub(bot->position, best_threat->position));
        return;
    }

    if ((best_prey != 0) && (best_prey_distance < (700.0f * 700.0f))) {
        bot->input_direction = ShroomNormalizeOrZero(ShroomVec2Sub(best_prey->position, bot->position));
        return;
    }

    for (index = 0; index < world->spore_count; ++index) {
        const ShroomSporeState *spore = &world->spores[index];
        const float distance_sqr = ShroomDistanceSqr(bot->position, spore->position);

        if (!spore->active) {
            continue;
        }

        if ((best_spore == 0) || (distance_sqr < best_spore_distance)) {
            best_spore = spore;
            best_spore_distance = distance_sqr;
        }
    }

    if (best_spore != 0) {
        bot->input_direction = ShroomNormalizeOrZero(ShroomVec2Sub(best_spore->position, bot->position));
        return;
    }

    bot->input_direction = ShroomNormalizeOrZero(ShroomVec2Sub(ShroomWorldCenter(world), bot->position));
}

static void ShroomCollectSpores(ShroomWorldState *world) {
    size_t player_index;
    size_t spore_index;

    for (player_index = 0; player_index < world->player_count; ++player_index) {
        ShroomPlayerState *player = &world->players[player_index];

        if (!player->alive) {
            continue;
        }

        for (spore_index = 0; spore_index < world->spore_count; ++spore_index) {
            ShroomSporeState *spore = &world->spores[spore_index];
            const float collection_radius = player->radius + 6.0f;

            if (!spore->active) {
                continue;
            }

            if (ShroomDistanceSqr(player->position, spore->position) <= (collection_radius * collection_radius)) {
                player->mass += (float)spore->value;
                ShroomSpawnOrResetSpore(world, spore);
            }
        }
    }
}

static bool ShroomCanConsume(const ShroomPlayerState *attacker, const ShroomPlayerState *target) {
    const float overlap_radius = attacker->radius * 0.88f;

    if (!attacker->alive || !target->alive) {
        return false;
    }
    if (attacker->mass < (target->mass * SHROOM_CONSUME_MASS_ADVANTAGE)) {
        return false;
    }

    return ShroomDistanceSqr(attacker->position, target->position) <= (overlap_radius * overlap_radius);
}

static void ShroomResolveConsumes(ShroomWorldState *world) {
    size_t attacker_index;
    bool consumed[SHROOM_MAX_PLAYERS] = {0};
    size_t consumed_by[SHROOM_MAX_PLAYERS] = {0};

    for (attacker_index = 0; attacker_index < world->player_count; ++attacker_index) {
        size_t target_index;
        const ShroomPlayerState *attacker = &world->players[attacker_index];

        if (!attacker->alive || consumed[attacker_index]) {
            continue;
        }

        for (target_index = 0; target_index < world->player_count; ++target_index) {
            const ShroomPlayerState *target = &world->players[target_index];

            if ((attacker_index == target_index) || consumed[target_index]) {
                continue;
            }

            if (ShroomCanConsume(attacker, target)) {
                consumed[target_index] = true;
                consumed_by[target_index] = attacker_index;
            }
        }
    }

    for (attacker_index = 0; attacker_index < world->player_count; ++attacker_index) {
        if (consumed[attacker_index]) {
            ShroomPlayerState *victim = &world->players[attacker_index];
            ShroomPlayerState *winner = &world->players[consumed_by[attacker_index]];

            winner->mass += victim->mass * SHROOM_CONSUME_MASS_GAIN_FACTOR;
            victim->alive = false;
            ShroomRespawnPlayer(world, victim);
        }
    }
}

float ShroomMassToRadius(float mass) {
    return 10.0f + (mass * 0.14f);
}

float ShroomMassToSpeed(float mass) {
    const float scaled_mass = mass * SHROOM_PLAYER_SPEED_MASS_SCALE;
    const float speed = SHROOM_MAX_PLAYER_SPEED / (1.0f + (scaled_mass / 100.0f));

    return ShroomClamp(speed, SHROOM_MIN_PLAYER_SPEED, SHROOM_MAX_PLAYER_SPEED);
}

void ShroomWorldInit(ShroomWorldState *world) {
    *world = (ShroomWorldState){0};
    world->width = SHROOM_WORLD_WIDTH;
    world->height = SHROOM_WORLD_HEIGHT;
    world->next_entity_id = 1;
    srand(7);
    ShroomInitializeSpores(world);
}

ShroomPlayerState *ShroomWorldSpawnPlayer(ShroomWorldState *world, ShroomPlayerId player_id, bool is_bot) {
    ShroomPlayerState *player;

    if (world->player_count >= SHROOM_MAX_PLAYERS) {
        return 0;
    }

    player = &world->players[world->player_count++];
    *player = (ShroomPlayerState){
        .player_id = player_id,
        .entity_id = world->next_entity_id++,
        .position = ShroomRandomSpawnPosition(world, true),
        .mass = SHROOM_DEFAULT_PLAYER_MASS,
        .radius = ShroomMassToRadius(SHROOM_DEFAULT_PLAYER_MASS),
        .alive = true,
        .is_bot = is_bot,
    };

    return player;
}

void ShroomPlayerSetInput(ShroomPlayerState *player, ShroomVec2 input_direction) {
    if (player == 0) {
        return;
    }

    player->input_direction = input_direction;
}

void ShroomWorldStep(ShroomWorldState *world, float delta_time) {
    size_t index;

    for (index = 0; index < world->player_count; ++index) {
        ShroomPlayerState *player = &world->players[index];

        if (player->is_bot && player->alive) {
            ShroomUpdateBotInput(world, player);
        }
    }

    for (index = 0; index < world->player_count; ++index) {
        ShroomPlayerState *player = &world->players[index];
        const float speed = ShroomMassToSpeed(player->mass);
        const ShroomVec2 velocity = ShroomVec2Scale(player->input_direction, speed * delta_time);

        if (!player->alive) {
            continue;
        }

        player->position = ShroomVec2Add(player->position, velocity);
        player->radius = ShroomMassToRadius(player->mass);
        player->position.x = ShroomClamp(player->position.x, player->radius, world->width - player->radius);
        player->position.y = ShroomClamp(player->position.y, player->radius, world->height - player->radius);
    }

    ShroomCollectSpores(world);
    ShroomResolveConsumes(world);

    world->tick += 1;
}
