#include "world_replication.h"

#include <string.h>

static bool SporeEqual(const ShroomSnapshotSporeState* left,
                       const ShroomSnapshotSporeState* right) {
  return (left->entity_id == right->entity_id) && (left->position_x == right->position_x) &&
         (left->position_y == right->position_y) && (left->value == right->value);
}

static bool PowerupEqual(const ShroomSnapshotPowerupState* left,
                         const ShroomSnapshotPowerupState* right) {
  return (left->entity_id == right->entity_id) && (left->position_x == right->position_x) &&
         (left->position_y == right->position_y) && (left->type == right->type) &&
         (left->active == right->active);
}

static int FindSpore(const ShroomSnapshotSporeState* spores, uint16_t count,
                     ShroomEntityId entity_id) {
  for (uint16_t index = 0u; index < count; ++index) {
    if (spores[index].entity_id == entity_id) {
      return (int)index;
    }
  }
  return -1;
}

static int FindPowerup(const ShroomSnapshotPowerupState* powerups, uint16_t count,
                       ShroomEntityId entity_id) {
  for (uint16_t index = 0u; index < count; ++index) {
    if (powerups[index].entity_id == entity_id) {
      return (int)index;
    }
  }
  return -1;
}

static bool AppendRecord(ShroomWorldStateRecord* records, size_t capacity, size_t* count,
                         ShroomWorldStateRecord record) {
  if (*count >= capacity) {
    return false;
  }
  records[(*count)++] = record;
  return true;
}

bool ShroomWorldReplicationInInterest(ShroomVec2 center, ShroomVec2 position) {
  const float radius = SHROOM_WORLD_REPLICATION_INTEREST_RADIUS;
  const float delta_x = center.x - position.x;
  const float delta_y = center.y - position.y;
  return delta_x * delta_x + delta_y * delta_y <= radius * radius;
}

ShroomWorldReplicationBatch ShroomWorldReplicationBuild(ShroomWorldReplicationPeerState* peer_state,
                                                        const ShroomWorldState* world,
                                                        ShroomVec2 interest_center,
                                                        bool force_keyframe,
                                                        ShroomWorldStateRecord* records,
                                                        size_t record_capacity) {
  size_t record_count = 0u;
  bool keyframe;

  if ((peer_state == NULL) || (world == NULL) || (records == NULL)) {
    return (ShroomWorldReplicationBatch){0};
  }

  keyframe =
      force_keyframe || !peer_state->initialized ||
      (world->tick < peer_state->last_keyframe_tick) ||
      ((world->tick - peer_state->last_keyframe_tick) >= SHROOM_WORLD_REPLICATION_KEYFRAME_TICKS);
  if (!keyframe) {
    for (size_t index = 0u; index < world->spore_count; ++index) {
      const ShroomSporeState* spore = &world->spores[index];
      if (peer_state->spore_known[index] && spore->active &&
          ShroomWorldReplicationInInterest(interest_center, spore->position) &&
          (peer_state->spores[index].entity_id != spore->entity_id)) {
        keyframe = true;
        break;
      }
    }
  }
  if (!keyframe) {
    for (size_t index = 0u; index < world->powerup_count; ++index) {
      const ShroomPowerupState* powerup = &world->powerups[index];
      if (peer_state->powerup_known[index] && powerup->active &&
          ShroomWorldReplicationInInterest(interest_center, powerup->position) &&
          (peer_state->powerups[index].entity_id != powerup->entity_id)) {
        keyframe = true;
        break;
      }
    }
  }
  for (size_t index = 0u; index < SHROOM_MAX_SPORES; ++index) {
    const ShroomSporeState* spore = &world->spores[index];
    const bool current = (index < world->spore_count) && spore->active &&
                         ShroomWorldReplicationInInterest(interest_center, spore->position);
    const ShroomSnapshotSporeState snapshot = {
        .entity_id = spore->entity_id,
        .position_x = spore->position.x,
        .position_y = spore->position.y,
        .value = spore->value,
    };
    uint8_t operation = 0u;

    if (current) {
      operation = keyframe || !peer_state->spore_known[index]          ? SHROOM_WORLD_RECORD_SPAWN
                  : !SporeEqual(&snapshot, &peer_state->spores[index]) ? SHROOM_WORLD_RECORD_UPDATE
                                                                       : 0u;
    } else if (!keyframe && peer_state->spore_known[index]) {
      operation = SHROOM_WORLD_RECORD_REMOVE;
    }
    if ((operation != 0u) &&
        !AppendRecord(
            records, record_capacity, &record_count,
            (ShroomWorldStateRecord){.entity_id = current ? snapshot.entity_id
                                                          : peer_state->spores[index].entity_id,
                                     .position_x = snapshot.position_x,
                                     .position_y = snapshot.position_y,
                                     .value = snapshot.value,
                                     .entity_kind = SHROOM_WORLD_ENTITY_SPORE,
                                     .operation = operation})) {
      return (ShroomWorldReplicationBatch){0};
    }
    peer_state->spore_known[index] = current;
    if (current) {
      peer_state->spores[index] = snapshot;
    }
  }
  for (size_t index = 0u; index < SHROOM_MAX_POWERUPS; ++index) {
    const ShroomPowerupState* powerup = &world->powerups[index];
    const bool current = (index < world->powerup_count) && powerup->active &&
                         ShroomWorldReplicationInInterest(interest_center, powerup->position);
    const ShroomSnapshotPowerupState snapshot = {
        .entity_id = powerup->entity_id,
        .position_x = powerup->position.x,
        .position_y = powerup->position.y,
        .type = (uint8_t)powerup->type,
        .active = 1u,
    };
    uint8_t operation = 0u;

    if (current) {
      operation = keyframe || !peer_state->powerup_known[index] ? SHROOM_WORLD_RECORD_SPAWN
                  : !PowerupEqual(&snapshot, &peer_state->powerups[index])
                      ? SHROOM_WORLD_RECORD_UPDATE
                      : 0u;
    } else if (!keyframe && peer_state->powerup_known[index]) {
      operation = SHROOM_WORLD_RECORD_REMOVE;
    }
    if ((operation != 0u) &&
        !AppendRecord(
            records, record_capacity, &record_count,
            (ShroomWorldStateRecord){.entity_id = current ? snapshot.entity_id
                                                          : peer_state->powerups[index].entity_id,
                                     .position_x = snapshot.position_x,
                                     .position_y = snapshot.position_y,
                                     .entity_kind = SHROOM_WORLD_ENTITY_POWERUP,
                                     .operation = operation,
                                     .powerup_type = snapshot.type})) {
      return (ShroomWorldReplicationBatch){0};
    }
    peer_state->powerup_known[index] = current;
    if (current) {
      peer_state->powerups[index] = snapshot;
    }
  }
  peer_state->initialized = true;
  if (keyframe) {
    peer_state->last_keyframe_tick = world->tick;
  }
  return (ShroomWorldReplicationBatch){
      .tick = world->tick, .record_count = record_count, .keyframe = keyframe};
}

static bool IdInList(const ShroomEntityId* ids, uint16_t count, ShroomEntityId id) {
  for (uint16_t index = 0u; index < count; ++index) {
    if (ids[index] == id) {
      return true;
    }
  }
  return false;
}

static void RememberId(ShroomEntityId* ids, uint16_t* count, uint16_t capacity, ShroomEntityId id) {
  if (!IdInList(ids, *count, id) && (*count < capacity)) {
    ids[(*count)++] = id;
  }
}

static void RemoveSpore(ShroomSnapshotSporeState* spores, uint16_t* count, uint16_t index) {
  if (index + 1u < *count) {
    memmove(&spores[index], &spores[index + 1u], (size_t)(*count - index - 1u) * sizeof(spores[0]));
  }
  --*count;
  memset(&spores[*count], 0, sizeof(spores[0]));
}

static void RemovePowerup(ShroomSnapshotPowerupState* powerups, uint16_t* count, uint16_t index) {
  if (index + 1u < *count) {
    memmove(&powerups[index], &powerups[index + 1u],
            (size_t)(*count - index - 1u) * sizeof(powerups[0]));
  }
  --*count;
  memset(&powerups[*count], 0, sizeof(powerups[0]));
}

static void ApplyRecord(ShroomSnapshotSporeState* spores, uint16_t* spore_count,
                        ShroomSnapshotPowerupState* powerups, uint16_t* powerup_count,
                        const ShroomWorldStateRecord* record) {
  if (record->entity_kind == SHROOM_WORLD_ENTITY_SPORE) {
    int index = FindSpore(spores, *spore_count, record->entity_id);
    if (record->operation == SHROOM_WORLD_RECORD_REMOVE) {
      if (index >= 0) {
        RemoveSpore(spores, spore_count, (uint16_t)index);
      }
    } else if ((record->operation == SHROOM_WORLD_RECORD_SPAWN) ||
               (record->operation == SHROOM_WORLD_RECORD_UPDATE)) {
      if ((index < 0) && (*spore_count < SHROOM_MAX_SPORES)) {
        index = (int)(*spore_count)++;
      }
      if (index >= 0) {
        spores[index] = (ShroomSnapshotSporeState){.entity_id = record->entity_id,
                                                   .position_x = record->position_x,
                                                   .position_y = record->position_y,
                                                   .value = record->value};
      }
    }
  } else if (record->entity_kind == SHROOM_WORLD_ENTITY_POWERUP) {
    int index = FindPowerup(powerups, *powerup_count, record->entity_id);
    if (record->operation == SHROOM_WORLD_RECORD_REMOVE) {
      if (index >= 0) {
        RemovePowerup(powerups, powerup_count, (uint16_t)index);
      }
    } else if ((record->operation == SHROOM_WORLD_RECORD_SPAWN) ||
               (record->operation == SHROOM_WORLD_RECORD_UPDATE)) {
      if ((index < 0) && (*powerup_count < SHROOM_MAX_POWERUPS)) {
        index = (int)(*powerup_count)++;
      }
      if (index >= 0) {
        powerups[index] = (ShroomSnapshotPowerupState){.entity_id = record->entity_id,
                                                       .position_x = record->position_x,
                                                       .position_y = record->position_y,
                                                       .type = record->powerup_type,
                                                       .active = 1u};
      }
    }
  }
}

static bool KeyframeComplete(const ShroomWorldReplicationClientState* state) {
  for (uint16_t chunk = 0u; chunk < state->keyframe_chunk_count; ++chunk) {
    if ((state->keyframe_chunks[chunk / 64u] & (1ull << (chunk % 64u))) == 0u) {
      return false;
    }
  }
  return true;
}

bool ShroomWorldReplicationApplyPacket(ShroomWorldReplicationClientState* client_state,
                                       ShroomSnapshotSporeState* spores, uint16_t* spore_count,
                                       ShroomSnapshotPowerupState* powerups,
                                       uint16_t* powerup_count,
                                       const ShroomWorldStatePacket* packet, size_t packet_size) {
  const size_t header_size = offsetof(ShroomWorldStatePacket, records);
  bool keyframe;

  if ((client_state == NULL) || (spores == NULL) || (spore_count == NULL) || (powerups == NULL) ||
      (powerup_count == NULL) || (packet == NULL) || (packet_size < header_size) ||
      (packet->header.type != SHROOM_PACKET_WORLD_STATE) || (packet->header.size != packet_size) ||
      ((packet->flags & (uint8_t)~SHROOM_WORLD_STATE_FLAG_KEYFRAME) != 0u) ||
      (packet->chunk_count == 0u) || (packet->chunk_count > SHROOM_WORLD_REPLICATION_MAX_CHUNKS) ||
      (packet->chunk_index >= packet->chunk_count) ||
      (packet->record_count > ShroomWorldStatePacketMaxRecords()) ||
      (packet_size !=
       header_size + (size_t)packet->record_count * sizeof(ShroomWorldStateRecord)) ||
      (client_state->tick_received && (packet->tick < client_state->latest_tick))) {
    return false;
  }
  keyframe = (packet->flags & SHROOM_WORLD_STATE_FLAG_KEYFRAME) != 0u;

  for (uint8_t index = 0u; index < packet->record_count; ++index) {
    const ShroomWorldStateRecord* record = &packet->records[index];
    if (((record->entity_kind != SHROOM_WORLD_ENTITY_SPORE) &&
         (record->entity_kind != SHROOM_WORLD_ENTITY_POWERUP)) ||
        (record->operation < SHROOM_WORLD_RECORD_SPAWN) ||
        (record->operation > SHROOM_WORLD_RECORD_REMOVE) || (record->entity_id == 0u)) {
      return false;
    }
  }

  if (!client_state->tick_received || (packet->tick > client_state->latest_tick)) {
    client_state->tick_received = true;
    client_state->latest_tick = packet->tick;
    if (client_state->keyframe_pending && (client_state->keyframe_tick < packet->tick)) {
      client_state->keyframe_pending = false;
    }
  }
  if (keyframe) {
    if (!client_state->keyframe_pending || (packet->tick > client_state->keyframe_tick)) {
      client_state->keyframe_pending = true;
      client_state->keyframe_tick = packet->tick;
      client_state->keyframe_chunk_count = packet->chunk_count;
      client_state->keyframe_chunks[0] = 0u;
      client_state->keyframe_chunks[1] = 0u;
      client_state->keyframe_spore_count = 0u;
      client_state->keyframe_powerup_count = 0u;
    }
    if ((packet->tick != client_state->keyframe_tick) ||
        (packet->chunk_count != client_state->keyframe_chunk_count)) {
      return false;
    }
  }

  for (uint8_t index = 0u; index < packet->record_count; ++index) {
    const ShroomWorldStateRecord* record = &packet->records[index];
    ApplyRecord(spores, spore_count, powerups, powerup_count, record);
    if (keyframe && (record->operation != SHROOM_WORLD_RECORD_REMOVE)) {
      if (record->entity_kind == SHROOM_WORLD_ENTITY_SPORE) {
        RememberId(client_state->keyframe_spores, &client_state->keyframe_spore_count,
                   SHROOM_MAX_SPORES, record->entity_id);
      } else {
        RememberId(client_state->keyframe_powerups, &client_state->keyframe_powerup_count,
                   SHROOM_MAX_POWERUPS, record->entity_id);
      }
    }
  }
  if (!keyframe) {
    return true;
  }
  client_state->keyframe_chunks[packet->chunk_index / 64u] |= 1ull << (packet->chunk_index % 64u);
  if (!KeyframeComplete(client_state)) {
    return true;
  }

  for (uint16_t index = *spore_count; index > 0u; --index) {
    if (!IdInList(client_state->keyframe_spores, client_state->keyframe_spore_count,
                  spores[index - 1u].entity_id)) {
      RemoveSpore(spores, spore_count, (uint16_t)(index - 1u));
    }
  }
  for (uint16_t index = *powerup_count; index > 0u; --index) {
    if (!IdInList(client_state->keyframe_powerups, client_state->keyframe_powerup_count,
                  powerups[index - 1u].entity_id)) {
      RemovePowerup(powerups, powerup_count, (uint16_t)(index - 1u));
    }
  }
  client_state->keyframe_pending = false;
  return true;
}

size_t ShroomWorldReplicationPacketCount(size_t record_count) {
  const size_t per_packet = ShroomWorldStatePacketMaxRecords();
  return record_count == 0u ? 1u : (record_count + per_packet - 1u) / per_packet;
}

size_t ShroomWorldReplicationPacketBytes(size_t record_count) {
  return ShroomWorldReplicationPacketCount(record_count) *
             offsetof(ShroomWorldStatePacket, records) +
         record_count * sizeof(ShroomWorldStateRecord);
}
