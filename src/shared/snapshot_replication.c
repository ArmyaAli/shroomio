#include "snapshot_replication.h"

#include <float.h>
#include <string.h>

typedef struct SnapshotCandidate {
  uint16_t index;
  uint8_t priority;
  float distance_sqr;
  ShroomPlayerId player_id;
  uint8_t piece_index;
  ShroomEntityId entity_id;
} SnapshotCandidate;

static bool EntityWasKnown(const ShroomSnapshotInterestState* state, ShroomEntityId entity_id) {
  for (size_t index = 0u; index < SHROOM_MAX_PLAYER_ENTITIES; ++index) {
    if (state->known_entity_ids[index] == entity_id) {
      return true;
    }
  }
  return false;
}

static ShroomVec2 FindInterestCenter(const ShroomWorldState* world, ShroomPlayerId viewer_player_id,
                                     ShroomEntityId focus_entity_id, bool spectator,
                                     float* focus_mass) {
  const ShroomPlayerState* fallback = NULL;

  *focus_mass = 0.0f;
  for (size_t index = 0u; index < world->player_count; ++index) {
    const ShroomPlayerState* player = &world->players[index];
    if (!player->alive || (player->mass <= 0.0f)) {
      continue;
    }
    if ((fallback == NULL) && (player->piece_index == 0u)) {
      fallback = player;
    }
    if ((focus_entity_id != 0u) && (player->entity_id == focus_entity_id)) {
      *focus_mass = player->mass;
      return player->position;
    }
    if (!spectator && (player->player_id == viewer_player_id) && (player->piece_index == 0u)) {
      fallback = player;
    }
  }
  if (fallback != NULL) {
    *focus_mass = fallback->mass;
    return fallback->position;
  }
  return (ShroomVec2){world->width * 0.5f, world->height * 0.5f};
}

static int CompareCandidate(const SnapshotCandidate* left, const SnapshotCandidate* right) {
  if (left->priority != right->priority) {
    return left->priority < right->priority ? -1 : 1;
  }
  if (left->distance_sqr != right->distance_sqr) {
    return left->distance_sqr < right->distance_sqr ? -1 : 1;
  }
  if (left->player_id != right->player_id) {
    return left->player_id < right->player_id ? -1 : 1;
  }
  if (left->piece_index != right->piece_index) {
    return left->piece_index < right->piece_index ? -1 : 1;
  }
  if (left->entity_id != right->entity_id) {
    return left->entity_id < right->entity_id ? -1 : 1;
  }
  return 0;
}

static void SortCandidates(SnapshotCandidate* candidates, size_t count) {
  for (size_t index = 1u; index < count; ++index) {
    SnapshotCandidate candidate = candidates[index];
    size_t position = index;
    while ((position > 0u) && (CompareCandidate(&candidate, &candidates[position - 1u]) < 0)) {
      candidates[position] = candidates[position - 1u];
      --position;
    }
    candidates[position] = candidate;
  }
}

size_t ShroomSnapshotSelectPlayers(ShroomSnapshotInterestState* state,
                                   const ShroomWorldState* world, ShroomPlayerId viewer_player_id,
                                   ShroomEntityId focus_entity_id, bool spectator,
                                   uint16_t* selected_indices, size_t selected_capacity) {
  SnapshotCandidate candidates[SHROOM_MAX_PLAYER_ENTITIES];
  ShroomEntityId selected_ids[SHROOM_MAX_PLAYER_ENTITIES] = {0};
  float focus_mass;
  ShroomVec2 center;
  size_t count = 0u;

  if ((state == NULL) || (world == NULL) || (selected_indices == NULL)) {
    return 0u;
  }
  center = FindInterestCenter(world, viewer_player_id, focus_entity_id, spectator, &focus_mass);
  for (size_t index = 0u; (index < world->player_count) && (index < SHROOM_MAX_PLAYER_ENTITIES);
       ++index) {
    const ShroomPlayerState* player = &world->players[index];
    const float dx = player->position.x - center.x;
    const float dy = player->position.y - center.y;
    const float distance_sqr = dx * dx + dy * dy;
    const float base_radius = SHROOM_WORLD_REPLICATION_INTEREST_RADIUS;
    const float retained_radius = base_radius + SHROOM_SNAPSHOT_INTEREST_HYSTERESIS;
    const bool focused = (focus_entity_id != 0u) && (player->entity_id == focus_entity_id);
    const bool own_colony = !spectator && (player->player_id == viewer_player_id);
    const bool objective = (world->objective_controller_id != 0u) &&
                           (player->player_id == world->objective_controller_id);
    const bool threat = (focus_mass > 0.0f) && (player->player_id != viewer_player_id) &&
                        (player->mass >= focus_mass * SHROOM_CONSUME_MASS_ADVANTAGE) &&
                        (distance_sqr <= retained_radius * retained_radius);
    const bool nearby = distance_sqr <= base_radius * base_radius;
    const bool retained = EntityWasKnown(state, player->entity_id) &&
                          (distance_sqr <= retained_radius * retained_radius);
    uint8_t priority;

    if (!player->alive || (player->mass <= 0.0f) ||
        (!focused && !own_colony && !objective && !threat && !nearby && !retained)) {
      continue;
    }
    priority = (focused || own_colony) ? 0u : objective ? 1u : threat ? 2u : 3u;
    candidates[count++] = (SnapshotCandidate){.index = (uint16_t)index,
                                              .priority = priority,
                                              .distance_sqr = distance_sqr,
                                              .player_id = player->player_id,
                                              .piece_index = player->piece_index,
                                              .entity_id = player->entity_id};
  }
  SortCandidates(candidates, count);
  if (count > selected_capacity) {
    count = selected_capacity;
  }
  for (size_t index = 0u; index < count; ++index) {
    selected_indices[index] = candidates[index].index;
    selected_ids[index] = candidates[index].entity_id;
  }
  memcpy(state->known_entity_ids, selected_ids, sizeof(state->known_entity_ids));
  return count;
}

size_t ShroomSnapshotChunkCount(size_t player_count) {
  return player_count == 0u ? 1u
                            : (player_count + SHROOM_SNAPSHOT_PLAYERS_PER_CHUNK - 1u) /
                                  SHROOM_SNAPSHOT_PLAYERS_PER_CHUNK;
}

static ShroomSnapshotFrameMetadata PacketMetadata(const ShroomSnapshotPacket* packet) {
  ShroomSnapshotFrameMetadata metadata;
  memset(&metadata, 0, sizeof(metadata));
  metadata.tick = packet->tick;
  metadata.last_processed_input_sequence = packet->last_processed_input_sequence;
  metadata.player_id = packet->player_id;
  metadata.entity_id = packet->entity_id;
  metadata.match_phase = packet->match_phase;
  metadata.game_mode = packet->game_mode;
  metadata.match_time_remaining = packet->match_time_remaining;
  metadata.objective_target_score = packet->objective_target_score;
  metadata.objective_controller_id = packet->objective_controller_id;
  metadata.objective_contested = packet->objective_contested;
  memcpy(metadata.podium_player_ids, packet->podium_player_ids, sizeof(metadata.podium_player_ids));
  memcpy(metadata.podium_masses, packet->podium_masses, sizeof(metadata.podium_masses));
  return metadata;
}

static bool MetadataMatches(const ShroomSnapshotFrameMetadata* metadata,
                            const ShroomSnapshotPacket* packet) {
  const ShroomSnapshotFrameMetadata incoming = PacketMetadata(packet);
  return memcmp(metadata, &incoming, sizeof(incoming)) == 0;
}

ShroomSnapshotAssemblyResult
ShroomSnapshotAssemblyPush(ShroomSnapshotAssembly* assembly, const ShroomSnapshotPacket* packet,
                           size_t packet_size, ShroomSnapshotFrameMetadata* completed_metadata,
                           ShroomSnapshotPlayerState* completed_players,
                           uint16_t* completed_player_count) {
  const size_t header_size = offsetof(ShroomSnapshotPacket, players);
  size_t expected_chunks;
  size_t offset;
  uint16_t expected_count;
  uint32_t chunk_bit;

  if ((assembly == NULL) || (packet == NULL) || (completed_metadata == NULL) ||
      (completed_players == NULL) || (completed_player_count == NULL) ||
      (packet_size < header_size) || (packet->header.type != SHROOM_PACKET_SNAPSHOT) ||
      (packet->header.size != packet_size) ||
      (packet->total_player_count > SHROOM_MAX_SNAPSHOT_PLAYERS) || (packet->chunk_count == 0u) ||
      (packet->chunk_count > SHROOM_SNAPSHOT_MAX_CHUNKS) ||
      (packet->chunk_index >= packet->chunk_count)) {
    return SHROOM_SNAPSHOT_ASSEMBLY_REJECTED;
  }
  expected_chunks = ShroomSnapshotChunkCount(packet->total_player_count);
  offset = (size_t)packet->chunk_index * SHROOM_SNAPSHOT_PLAYERS_PER_CHUNK;
  expected_count =
      (uint16_t)((packet->total_player_count > offset + SHROOM_SNAPSHOT_PLAYERS_PER_CHUNK)
                     ? SHROOM_SNAPSHOT_PLAYERS_PER_CHUNK
                     : packet->total_player_count - offset);
  if ((packet->chunk_count != expected_chunks) || (packet->player_count != expected_count) ||
      (packet_size != header_size + (size_t)packet->player_count * sizeof(packet->players[0]))) {
    return SHROOM_SNAPSHOT_ASSEMBLY_REJECTED;
  }
  if (assembly->active && (packet->tick < assembly->tick)) {
    return SHROOM_SNAPSHOT_ASSEMBLY_REJECTED;
  }
  if (!assembly->active || (packet->tick > assembly->tick)) {
    *assembly = (ShroomSnapshotAssembly){.active = true,
                                         .tick = packet->tick,
                                         .chunk_count = packet->chunk_count,
                                         .total_player_count = packet->total_player_count,
                                         .metadata = PacketMetadata(packet)};
  } else if ((packet->chunk_count != assembly->chunk_count) ||
             (packet->total_player_count != assembly->total_player_count) ||
             !MetadataMatches(&assembly->metadata, packet)) {
    return SHROOM_SNAPSHOT_ASSEMBLY_REJECTED;
  }
  chunk_bit = 1u << packet->chunk_index;
  if ((assembly->received_chunks & chunk_bit) == 0u) {
    memcpy(&assembly->players[offset], packet->players,
           (size_t)packet->player_count * sizeof(packet->players[0]));
    assembly->received_chunks |= chunk_bit;
  }
  if (assembly->received_chunks != ((1u << assembly->chunk_count) - 1u)) {
    return SHROOM_SNAPSHOT_ASSEMBLY_PENDING;
  }
  *completed_metadata = assembly->metadata;
  *completed_player_count = assembly->total_player_count;
  memcpy(completed_players, assembly->players,
         (size_t)assembly->total_player_count * sizeof(assembly->players[0]));
  assembly->active = false;
  return SHROOM_SNAPSHOT_ASSEMBLY_COMPLETE;
}
