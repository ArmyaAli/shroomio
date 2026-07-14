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
  const size_t full_record_size =
      sizeof(ShroomSnapshotRecordHeader) + sizeof(uint32_t) + SHROOM_MAX_NAME_LENGTH + 5u +
      4u * sizeof(float) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint8_t) + sizeof(float);
  const size_t records_per_chunk = SHROOM_SNAPSHOT_PAYLOAD_SIZE / full_record_size;
  return player_count == 0u ? 1u : (player_count + records_per_chunk - 1u) / records_per_chunk;
}

bool ShroomSnapshotTickIsNewer(uint64_t candidate, uint64_t reference) {
  return (candidate != reference) && ((int64_t)(candidate - reference) > 0);
}

static const ShroomSnapshotPlayerState* FindPlayer(const ShroomSnapshotPlayerState* players,
                                                   uint16_t count, uint32_t entity_id) {
  for (uint16_t index = 0u; index < count; ++index) {
    if (players[index].entity_id == entity_id) {
      return &players[index];
    }
  }
  return NULL;
}

const ShroomSnapshotFrame* ShroomSnapshotHistoryFind(const ShroomSnapshotHistory* history,
                                                     uint64_t tick) {
  if ((history == NULL) || (tick == 0u)) {
    return NULL;
  }
  for (size_t index = 0u; index < SHROOM_SNAPSHOT_HISTORY_SIZE; ++index) {
    if (history->frames[index].tick == tick) {
      return &history->frames[index];
    }
  }
  return NULL;
}

void ShroomSnapshotHistoryStore(ShroomSnapshotHistory* history, uint64_t tick,
                                const ShroomSnapshotPlayerState* players, uint16_t player_count) {
  ShroomSnapshotFrame* frame;
  if ((history == NULL) || (players == NULL) || (tick == 0u) ||
      (player_count > SHROOM_MAX_SNAPSHOT_PLAYERS)) {
    return;
  }
  frame = &history->frames[history->next % SHROOM_SNAPSHOT_HISTORY_SIZE];
  *frame = (ShroomSnapshotFrame){.tick = tick, .player_count = player_count};
  memcpy(frame->players, players, (size_t)player_count * sizeof(players[0]));
  history->next = (uint8_t)((history->next + 1u) % SHROOM_SNAPSHOT_HISTORY_SIZE);
}

static uint16_t ComponentMask(const ShroomSnapshotPlayerState* current,
                              const ShroomSnapshotPlayerState* baseline) {
  uint16_t mask = 0u;
  if ((current->player_id != baseline->player_id) ||
      (memcmp(current->name, baseline->name, sizeof(current->name)) != 0) ||
      (current->alive != baseline->alive) || (current->is_bot != baseline->is_bot) ||
      (current->team_id != baseline->team_id) || (current->piece_index != baseline->piece_index) ||
      (current->life_generation != baseline->life_generation)) {
    mask |= SHROOM_SNAPSHOT_COMPONENT_IDENTITY;
  }
  if ((current->position_x != baseline->position_x) ||
      (current->position_y != baseline->position_y)) {
    mask |= SHROOM_SNAPSHOT_COMPONENT_TRANSFORM;
  }
  if ((current->mass != baseline->mass) || (current->radius != baseline->radius)) {
    mask |= SHROOM_SNAPSHOT_COMPONENT_MASS;
  }
  if (current->effect_flags != baseline->effect_flags) {
    mask |= SHROOM_SNAPSHOT_COMPONENT_EFFECTS;
  }
  if ((current->round_spores != baseline->round_spores) ||
      (current->round_kills != baseline->round_kills) ||
      (current->objective_score != baseline->objective_score)) {
    mask |= SHROOM_SNAPSHOT_COMPONENT_STATS;
  }
  return mask;
}

static bool WriteBytes(uint8_t* destination, size_t capacity, size_t* offset, const void* source,
                       size_t size) {
  if ((*offset + size) > capacity) {
    return false;
  }
  memcpy(destination + *offset, source, size);
  *offset += size;
  return true;
}

static size_t EncodeRecord(uint8_t* destination, size_t capacity, uint8_t operation, uint16_t mask,
                           uint32_t entity_id, const ShroomSnapshotPlayerState* player) {
  ShroomSnapshotRecordHeader header = {
      .entity_id = entity_id, .component_mask = mask, .operation = operation};
  size_t offset = sizeof(header);
  if (capacity < sizeof(header)) {
    return 0u;
  }
#define WRITE_FIELD(field)                                                                         \
  do {                                                                                             \
    if (!WriteBytes(destination, capacity, &offset, &(field), sizeof(field))) {                    \
      return 0u;                                                                                   \
    }                                                                                              \
  } while (0)
  if (operation != SHROOM_SNAPSHOT_RECORD_DESPAWN) {
    if ((mask & SHROOM_SNAPSHOT_COMPONENT_IDENTITY) != 0u) {
      WRITE_FIELD(player->player_id);
      if (!WriteBytes(destination, capacity, &offset, player->name, sizeof(player->name))) {
        return 0u;
      }
      WRITE_FIELD(player->alive);
      WRITE_FIELD(player->is_bot);
      WRITE_FIELD(player->team_id);
      WRITE_FIELD(player->piece_index);
      WRITE_FIELD(player->life_generation);
    }
    if ((mask & SHROOM_SNAPSHOT_COMPONENT_TRANSFORM) != 0u) {
      WRITE_FIELD(player->position_x);
      WRITE_FIELD(player->position_y);
    }
    if ((mask & SHROOM_SNAPSHOT_COMPONENT_MASS) != 0u) {
      WRITE_FIELD(player->mass);
      WRITE_FIELD(player->radius);
    }
    if ((mask & SHROOM_SNAPSHOT_COMPONENT_EFFECTS) != 0u) {
      WRITE_FIELD(player->effect_flags);
    }
    if ((mask & SHROOM_SNAPSHOT_COMPONENT_STATS) != 0u) {
      WRITE_FIELD(player->round_spores);
      WRITE_FIELD(player->round_kills);
      WRITE_FIELD(player->objective_score);
    }
  }
#undef WRITE_FIELD
  if (offset > UINT8_MAX) {
    return 0u;
  }
  header.size = (uint8_t)offset;
  memcpy(destination, &header, sizeof(header));
  return offset;
}

static void InitPacket(ShroomSnapshotPacket* packet, const ShroomSnapshotFrameMetadata* metadata,
                       uint64_t baseline_tick, uint16_t player_count, bool keyframe) {
  *packet = (ShroomSnapshotPacket){
      .tick = metadata->tick,
      .baseline_tick = baseline_tick,
      .last_processed_input_sequence = metadata->last_processed_input_sequence,
      .player_id = metadata->player_id,
      .entity_id = metadata->entity_id,
      .total_player_count = player_count,
      .flags = keyframe ? SHROOM_SNAPSHOT_FLAG_KEYFRAME : 0u,
      .match_phase = metadata->match_phase,
      .game_mode = metadata->game_mode,
      .match_time_remaining = metadata->match_time_remaining,
      .objective_target_score = metadata->objective_target_score,
      .objective_controller_id = metadata->objective_controller_id,
      .objective_contested = metadata->objective_contested,
  };
  memcpy(packet->podium_player_ids, metadata->podium_player_ids, sizeof(packet->podium_player_ids));
  memcpy(packet->podium_masses, metadata->podium_masses, sizeof(packet->podium_masses));
}

static bool AppendRecord(ShroomSnapshotEncodedFrame* encoded,
                         const ShroomSnapshotFrameMetadata* metadata, uint64_t baseline_tick,
                         uint16_t player_count, bool keyframe, const uint8_t* record,
                         size_t record_size) {
  ShroomSnapshotPacket* packet;
  if ((record_size == 0u) || (record_size > SHROOM_SNAPSHOT_PAYLOAD_SIZE)) {
    return false;
  }
  if (encoded->packet_count == 0u) {
    encoded->packet_count = 1u;
    InitPacket(&encoded->packets[0], metadata, baseline_tick, player_count, keyframe);
  }
  packet = &encoded->packets[encoded->packet_count - 1u];
  if ((size_t)packet->payload_size + record_size > SHROOM_SNAPSHOT_PAYLOAD_SIZE) {
    if (encoded->packet_count >= SHROOM_SNAPSHOT_MAX_CHUNKS) {
      return false;
    }
    packet = &encoded->packets[encoded->packet_count++];
    InitPacket(packet, metadata, baseline_tick, player_count, keyframe);
  }
  memcpy(packet->payload + packet->payload_size, record, record_size);
  packet->payload_size = (uint16_t)(packet->payload_size + record_size);
  ++packet->record_count;
  return true;
}

bool ShroomSnapshotEncodeFrame(const ShroomSnapshotFrameMetadata* metadata,
                               const ShroomSnapshotPlayerState* players, uint16_t player_count,
                               const ShroomSnapshotFrame* baseline, bool keyframe,
                               ShroomSnapshotEncodedFrame* encoded) {
  uint8_t record[128];
  if ((metadata == NULL) || (players == NULL) || (encoded == NULL) ||
      (player_count > SHROOM_MAX_SNAPSHOT_PLAYERS) || (!keyframe && (baseline == NULL))) {
    return false;
  }
  memset(encoded, 0, sizeof(*encoded));
  for (uint16_t index = 0u; index < player_count; ++index) {
    const ShroomSnapshotPlayerState* previous =
        keyframe ? NULL
                 : FindPlayer(baseline->players, baseline->player_count, players[index].entity_id);
    const uint8_t operation =
        previous == NULL ? SHROOM_SNAPSHOT_RECORD_SPAWN : SHROOM_SNAPSHOT_RECORD_UPDATE;
    const uint16_t mask =
        previous == NULL ? SHROOM_SNAPSHOT_COMPONENT_ALL : ComponentMask(&players[index], previous);
    size_t size;
    if (mask == 0u) {
      continue;
    }
    size = EncodeRecord(record, sizeof(record), operation, mask, players[index].entity_id,
                        &players[index]);
    if (!AppendRecord(encoded, metadata, keyframe ? 0u : baseline->tick, player_count, keyframe,
                      record, size)) {
      return false;
    }
  }
  if (!keyframe) {
    for (uint16_t index = 0u; index < baseline->player_count; ++index) {
      size_t size;
      if (FindPlayer(players, player_count, baseline->players[index].entity_id) != NULL) {
        continue;
      }
      size = EncodeRecord(record, sizeof(record), SHROOM_SNAPSHOT_RECORD_DESPAWN, 0u,
                          baseline->players[index].entity_id, NULL);
      if (!AppendRecord(encoded, metadata, baseline->tick, player_count, false, record, size)) {
        return false;
      }
    }
  }
  if (encoded->packet_count == 0u) {
    encoded->packet_count = 1u;
    InitPacket(&encoded->packets[0], metadata, keyframe ? 0u : baseline->tick, player_count,
               keyframe);
  }
  for (uint16_t index = 0u; index < encoded->packet_count; ++index) {
    ShroomSnapshotPacket* packet = &encoded->packets[index];
    const size_t wire_size = offsetof(ShroomSnapshotPacket, payload) + packet->payload_size;
    packet->chunk_index = index;
    packet->chunk_count = encoded->packet_count;
    ShroomPacketHeaderInit(&packet->header, SHROOM_PACKET_SNAPSHOT, (uint16_t)wire_size);
    encoded->wire_bytes += wire_size;
  }
  return true;
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

static bool ReadBytes(const uint8_t* source, size_t size, size_t* offset, void* destination,
                      size_t field_size) {
  if ((*offset + field_size) > size) {
    return false;
  }
  memcpy(destination, source + *offset, field_size);
  *offset += field_size;
  return true;
}

static bool DecodeRecord(const uint8_t* record, size_t available,
                         ShroomSnapshotPlayerState* players, uint16_t* player_count) {
  ShroomSnapshotRecordHeader header;
  ShroomSnapshotPlayerState* player = NULL;
  size_t offset = sizeof(header);
  uint16_t index;
  if (available < sizeof(header)) {
    return false;
  }
  memcpy(&header, record, sizeof(header));
  if ((header.size < sizeof(header)) || (header.size > available) ||
      ((header.component_mask & ~SHROOM_SNAPSHOT_COMPONENT_ALL) != 0u)) {
    return false;
  }
  for (index = 0u; index < *player_count; ++index) {
    if (players[index].entity_id == header.entity_id) {
      player = &players[index];
      break;
    }
  }
  if (header.operation == SHROOM_SNAPSHOT_RECORD_DESPAWN) {
    if ((header.component_mask != 0u) || (header.size != sizeof(header)) || (player == NULL)) {
      return false;
    }
    memmove(&players[index], &players[index + 1u],
            (size_t)(*player_count - index - 1u) * sizeof(players[0]));
    --*player_count;
    return true;
  }
  if ((header.operation != SHROOM_SNAPSHOT_RECORD_SPAWN) &&
      (header.operation != SHROOM_SNAPSHOT_RECORD_UPDATE)) {
    return false;
  }
  if (header.operation == SHROOM_SNAPSHOT_RECORD_SPAWN) {
    if ((player != NULL) || (*player_count >= SHROOM_MAX_SNAPSHOT_PLAYERS) ||
        (header.component_mask != SHROOM_SNAPSHOT_COMPONENT_ALL)) {
      return false;
    }
    player = &players[(*player_count)++];
    *player = (ShroomSnapshotPlayerState){.entity_id = header.entity_id};
  } else if (player == NULL) {
    return false;
  }
#define READ_FIELD(field)                                                                          \
  do {                                                                                             \
    if (!ReadBytes(record, header.size, &offset, &(field), sizeof(field))) {                       \
      return false;                                                                                \
    }                                                                                              \
  } while (0)
  if ((header.component_mask & SHROOM_SNAPSHOT_COMPONENT_IDENTITY) != 0u) {
    READ_FIELD(player->player_id);
    if (!ReadBytes(record, header.size, &offset, player->name, sizeof(player->name))) {
      return false;
    }
    player->name[sizeof(player->name) - 1u] = '\0';
    READ_FIELD(player->alive);
    READ_FIELD(player->is_bot);
    READ_FIELD(player->team_id);
    READ_FIELD(player->piece_index);
    READ_FIELD(player->life_generation);
  }
  if ((header.component_mask & SHROOM_SNAPSHOT_COMPONENT_TRANSFORM) != 0u) {
    READ_FIELD(player->position_x);
    READ_FIELD(player->position_y);
  }
  if ((header.component_mask & SHROOM_SNAPSHOT_COMPONENT_MASS) != 0u) {
    READ_FIELD(player->mass);
    READ_FIELD(player->radius);
  }
  if ((header.component_mask & SHROOM_SNAPSHOT_COMPONENT_EFFECTS) != 0u) {
    READ_FIELD(player->effect_flags);
  }
  if ((header.component_mask & SHROOM_SNAPSHOT_COMPONENT_STATS) != 0u) {
    READ_FIELD(player->round_spores);
    READ_FIELD(player->round_kills);
    READ_FIELD(player->objective_score);
  }
#undef READ_FIELD
  return offset == header.size;
}

static bool ApplyChunks(const ShroomSnapshotAssembly* assembly,
                        const ShroomSnapshotHistory* history,
                        ShroomSnapshotPlayerState* completed_players,
                        uint16_t* completed_player_count) {
  const ShroomSnapshotPacket* first = &assembly->chunks[0];
  const bool keyframe = (first->flags & SHROOM_SNAPSHOT_FLAG_KEYFRAME) != 0u;
  const ShroomSnapshotFrame* baseline =
      keyframe ? NULL : ShroomSnapshotHistoryFind(history, first->baseline_tick);
  uint16_t count = 0u;
  if (!keyframe) {
    if (baseline == NULL) {
      return false;
    }
    count = baseline->player_count;
    memcpy(completed_players, baseline->players, (size_t)count * sizeof(completed_players[0]));
  }
  for (uint16_t chunk_index = 0u; chunk_index < assembly->chunk_count; ++chunk_index) {
    const ShroomSnapshotPacket* packet = &assembly->chunks[chunk_index];
    size_t offset = 0u;
    uint16_t records = 0u;
    while (offset < packet->payload_size) {
      ShroomSnapshotRecordHeader header;
      if ((packet->payload_size - offset < sizeof(header)) ||
          !ReadBytes(packet->payload, packet->payload_size, &offset, &header, sizeof(header)) ||
          (header.size < sizeof(header)) ||
          ((size_t)header.size > packet->payload_size - (offset - sizeof(header))) ||
          !DecodeRecord(packet->payload + offset - sizeof(header),
                        packet->payload_size - offset + sizeof(header), completed_players,
                        &count)) {
        return false;
      }
      offset += (size_t)header.size - sizeof(header);
      ++records;
    }
    if (records != packet->record_count) {
      return false;
    }
  }
  if (count != first->total_player_count) {
    return false;
  }
  *completed_player_count = count;
  return true;
}

ShroomSnapshotAssemblyResult ShroomSnapshotAssemblyPush(
    ShroomSnapshotAssembly* assembly, const ShroomSnapshotPacket* packet, size_t packet_size,
    ShroomSnapshotHistory* history, ShroomSnapshotFrameMetadata* completed_metadata,
    ShroomSnapshotPlayerState* completed_players, uint16_t* completed_player_count) {
  const size_t header_size = offsetof(ShroomSnapshotPacket, payload);
  uint32_t chunk_bit;

  if ((assembly == NULL) || (packet == NULL) || (completed_metadata == NULL) || (history == NULL) ||
      (completed_players == NULL) || (completed_player_count == NULL) ||
      (packet_size < header_size) || (packet->header.type != SHROOM_PACKET_SNAPSHOT) ||
      (packet->header.size != packet_size) ||
      (packet->total_player_count > SHROOM_MAX_SNAPSHOT_PLAYERS) || (packet->chunk_count == 0u) ||
      (packet->chunk_count > SHROOM_SNAPSHOT_MAX_CHUNKS) ||
      (packet->chunk_index >= packet->chunk_count) ||
      (packet->payload_size > SHROOM_SNAPSHOT_PAYLOAD_SIZE) ||
      (packet_size != header_size + packet->payload_size) ||
      ((packet->flags & ~SHROOM_SNAPSHOT_FLAG_KEYFRAME) != 0u) ||
      (((packet->flags & SHROOM_SNAPSHOT_FLAG_KEYFRAME) != 0u) != (packet->baseline_tick == 0u))) {
    return SHROOM_SNAPSHOT_ASSEMBLY_REJECTED;
  }
  if (assembly->active && (packet->tick != assembly->tick) &&
      !ShroomSnapshotTickIsNewer(packet->tick, assembly->tick)) {
    return SHROOM_SNAPSHOT_ASSEMBLY_REJECTED;
  }
  if (!assembly->active || ((packet->tick != assembly->tick) &&
                            ShroomSnapshotTickIsNewer(packet->tick, assembly->tick))) {
    *assembly = (ShroomSnapshotAssembly){.active = true,
                                         .tick = packet->tick,
                                         .baseline_tick = packet->baseline_tick,
                                         .chunk_count = packet->chunk_count,
                                         .total_player_count = packet->total_player_count,
                                         .flags = packet->flags,
                                         .metadata = PacketMetadata(packet)};
  } else if ((packet->chunk_count != assembly->chunk_count) ||
             (packet->total_player_count != assembly->total_player_count) ||
             (packet->baseline_tick != assembly->baseline_tick) ||
             (packet->flags != assembly->flags) || !MetadataMatches(&assembly->metadata, packet)) {
    return SHROOM_SNAPSHOT_ASSEMBLY_REJECTED;
  }
  chunk_bit = 1u << packet->chunk_index;
  if ((assembly->received_chunks & chunk_bit) == 0u) {
    assembly->chunks[packet->chunk_index] = *packet;
    assembly->received_chunks |= chunk_bit;
  }
  if (assembly->received_chunks !=
      (assembly->chunk_count == 32u ? UINT32_MAX : (1u << assembly->chunk_count) - 1u)) {
    return SHROOM_SNAPSHOT_ASSEMBLY_PENDING;
  }
  *completed_metadata = assembly->metadata;
  if (!ApplyChunks(assembly, history, completed_players, completed_player_count)) {
    assembly->active = false;
    return SHROOM_SNAPSHOT_ASSEMBLY_REJECTED;
  }
  ShroomSnapshotHistoryStore(history, completed_metadata->tick, completed_players,
                             *completed_player_count);
  assembly->active = false;
  return SHROOM_SNAPSHOT_ASSEMBLY_COMPLETE;
}
