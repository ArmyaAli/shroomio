#include "match_persistence.h"

#include <stdio.h>
#include <string.h>

static bool Execute(sqlite3* db, const char* sql) {
  return sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK;
}

bool ShroomMatchPersistenceBegin(sqlite3* db, const ShroomCompletedMatch* match) {
  static const char* const sql =
      "INSERT OR IGNORE INTO sessions(session_uuid,player_count,bot_count,lobby_id,round_id,"
      "game_mode,winner_runtime_player_id,status) VALUES(?1,?2,?3,?4,?5,?6,?7,'active')";
  sqlite3_stmt* statement = NULL;
  bool success;

  if ((db == NULL) || (match == NULL) || (match->session_uuid == NULL) ||
      (match->session_uuid[0] == '\0') || sqlite3_prepare_v2(db, sql, -1, &statement, NULL) !=
                                             SQLITE_OK) {
    return false;
  }
  success = sqlite3_bind_text(statement, 1, match->session_uuid, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
            sqlite3_bind_int64(statement, 2, (sqlite3_int64)match->participant_count) == SQLITE_OK &&
            sqlite3_bind_int(statement, 3, match->bot_count) == SQLITE_OK &&
            sqlite3_bind_int64(statement, 4, match->lobby_id) == SQLITE_OK &&
            sqlite3_bind_int64(statement, 5, match->round_id) == SQLITE_OK &&
            sqlite3_bind_int(statement, 6, match->game_mode) == SQLITE_OK &&
            sqlite3_bind_int64(statement, 7, match->winner_runtime_player_id) == SQLITE_OK &&
            sqlite3_step(statement) == SQLITE_DONE;
  sqlite3_finalize(statement);
  return success;
}

bool ShroomMatchPersistenceRecordEvent(sqlite3* db, const char* session_uuid,
                                       const char* event_type, uint64_t tick_number,
                                       int64_t actor_player_id, int64_t target_player_id,
                                       float mass_value, float position_x, float position_y,
                                       const char* metadata) {
  static const char* const sql =
      "INSERT INTO match_events(session_id,event_type,tick_number,actor_player_id,target_player_id,"
      "mass_value,position_x,position_y,metadata) SELECT id,?2,?3,NULLIF(?4,0),NULLIF(?5,0),"
      "?6,?7,?8,?9 FROM sessions WHERE session_uuid=?1 AND status='active'";
  sqlite3_stmt* statement = NULL;
  bool success = false;

  if ((db == NULL) || (session_uuid == NULL) || (session_uuid[0] == '\0') ||
      (event_type == NULL) || (event_type[0] == '\0') ||
      sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) {
    return false;
  }
  if (sqlite3_bind_text(statement, 1, session_uuid, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
      sqlite3_bind_text(statement, 2, event_type, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
      sqlite3_bind_int64(statement, 3, (sqlite3_int64)tick_number) == SQLITE_OK &&
      sqlite3_bind_int64(statement, 4, actor_player_id) == SQLITE_OK &&
      sqlite3_bind_int64(statement, 5, target_player_id) == SQLITE_OK &&
      sqlite3_bind_double(statement, 6, mass_value) == SQLITE_OK &&
      sqlite3_bind_double(statement, 7, position_x) == SQLITE_OK &&
      sqlite3_bind_double(statement, 8, position_y) == SQLITE_OK &&
      sqlite3_bind_text(statement, 9, metadata != NULL ? metadata : "", -1, SQLITE_TRANSIENT) ==
          SQLITE_OK &&
      sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1) {
    success = true;
  }
  sqlite3_finalize(statement);
  return success;
}

static bool BindParticipant(sqlite3_stmt* statement, int64_t session_id,
                            const ShroomPersistedParticipant* participant) {
  return sqlite3_bind_int64(statement, 1, session_id) == SQLITE_OK &&
         sqlite3_bind_int64(statement, 2, participant->database_player_id) == SQLITE_OK &&
         sqlite3_bind_int64(statement, 3, participant->runtime_player_id) == SQLITE_OK &&
         sqlite3_bind_int(statement, 4, participant->disconnected ? 1 : 0) == SQLITE_OK &&
         sqlite3_bind_int(statement, 5, participant->final_rank) == SQLITE_OK &&
         sqlite3_bind_double(statement, 6, participant->final_mass) == SQLITE_OK &&
         sqlite3_bind_int64(statement, 7, participant->round_stats.kills) == SQLITE_OK &&
         sqlite3_bind_int64(statement, 8, participant->round_stats.deaths) == SQLITE_OK &&
         sqlite3_bind_double(statement, 9, participant->round_stats.mass_consumed) == SQLITE_OK &&
         sqlite3_bind_double(statement, 10, participant->round_stats.mass_lost) == SQLITE_OK &&
         sqlite3_bind_double(statement, 11, participant->round_stats.distance_traveled) == SQLITE_OK &&
         sqlite3_bind_int64(statement, 12, participant->round_stats.spores_collected) == SQLITE_OK &&
         sqlite3_bind_int64(statement, 13, participant->round_stats.powerups_collected) ==
             SQLITE_OK &&
         sqlite3_bind_double(statement, 14, participant->round_stats.peak_mass) == SQLITE_OK &&
         sqlite3_bind_double(statement, 15, participant->round_stats.center_zone_seconds) ==
             SQLITE_OK &&
         sqlite3_bind_double(statement, 16, participant->round_stats.mid_zone_seconds) ==
             SQLITE_OK &&
         sqlite3_bind_double(statement, 17, participant->round_stats.outer_zone_seconds) ==
             SQLITE_OK &&
         sqlite3_bind_int64(statement, 18, participant->round_stats.splits_used) == SQLITE_OK &&
         sqlite3_bind_int64(statement, 19, participant->round_stats.ejects_used) == SQLITE_OK;
}

static bool SaveParticipant(sqlite3* db, int64_t session_id, uint32_t duration_seconds,
                            const ShroomPersistedParticipant* participant, uint64_t final_tick) {
  static const char* const participant_sql =
      "INSERT INTO session_participants (session_id,player_id,runtime_player_id,left_at,"
      "disconnected,final_rank,final_mass,kills,deaths,mass_consumed,mass_lost,distance_traveled,"
      "spores_collected,powerups_collected,peak_mass,center_zone_seconds,mid_zone_seconds,"
      "outer_zone_seconds,splits_used,ejects_used) VALUES "
      "(?1,?2,?3,CASE WHEN ?4 THEN strftime('%Y-%m-%dT%H:%M:%SZ','now') ELSE NULL END,"
      "?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19)";
  static const char* const stats_sql =
      "INSERT INTO player_stats (player_id,total_games_played,total_kills,total_deaths,total_mass_consumed,"
      "total_mass_lost,total_distance_traveled,total_spores_consumed,"
      "total_players_consumed,highest_mass_achieved,highest_rank_achieved,"
      "longest_survival_seconds) VALUES (?1,1,?2,?3,?4,?5,?6,?7,?2,?8,?9,?10) ON CONFLICT(player_id) DO "
      "UPDATE SET "
      "total_games_played=total_games_played+1,total_kills=total_kills+excluded.total_kills,"
      "total_deaths=total_deaths+excluded.total_deaths,total_mass_consumed=total_mass_consumed+excluded.total_mass_consumed,"
      "total_mass_lost=total_mass_lost+excluded.total_mass_lost,total_distance_traveled=total_distance_traveled+excluded.total_distance_traveled,"
      "total_spores_consumed=total_spores_consumed+excluded.total_spores_consumed,"
      "total_players_consumed=total_players_consumed+excluded.total_players_consumed,"
      "highest_mass_achieved=max(highest_mass_achieved,excluded.highest_mass_achieved),"
      "highest_rank_achieved=min(highest_rank_achieved,excluded.highest_rank_achieved),"
      "longest_survival_seconds=max(longest_survival_seconds,excluded.longest_survival_seconds),"
      "updated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now')";
  static const char* const player_sql =
      "UPDATE players SET total_sessions=total_sessions+1,total_play_time_seconds="
      "total_play_time_seconds+?2,last_seen_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') WHERE id=?1";
  static const char* const event_sql =
      "INSERT INTO match_events (session_id,event_type,tick_number,actor_player_id,mass_value,"
      "metadata) VALUES (?1,'participant_summary',?2,?3,?4,?5)";
  sqlite3_stmt* statement = NULL;
  char metadata[192];
  bool success = false;

  if (sqlite3_prepare_v2(db, participant_sql, -1, &statement, NULL) != SQLITE_OK ||
      !BindParticipant(statement, session_id, participant) ||
      sqlite3_step(statement) != SQLITE_DONE) {
    goto cleanup;
  }
  sqlite3_finalize(statement);
  statement = NULL;

  if (sqlite3_prepare_v2(db, stats_sql, -1, &statement, NULL) != SQLITE_OK ||
      sqlite3_bind_int64(statement, 1, participant->database_player_id) != SQLITE_OK ||
      sqlite3_bind_int64(statement, 2, participant->round_stats.kills) != SQLITE_OK ||
      sqlite3_bind_int64(statement, 3, participant->round_stats.deaths) != SQLITE_OK ||
      sqlite3_bind_double(statement, 4, participant->round_stats.mass_consumed) != SQLITE_OK ||
      sqlite3_bind_double(statement, 5, participant->round_stats.mass_lost) != SQLITE_OK ||
      sqlite3_bind_double(statement, 6, participant->round_stats.distance_traveled) != SQLITE_OK ||
      sqlite3_bind_int64(statement, 7, participant->round_stats.spores_collected) != SQLITE_OK ||
      sqlite3_bind_double(statement, 8, participant->round_stats.peak_mass) != SQLITE_OK ||
      sqlite3_bind_int(statement, 9, participant->final_rank) != SQLITE_OK ||
      sqlite3_bind_int64(statement, 10, duration_seconds) != SQLITE_OK ||
      sqlite3_step(statement) != SQLITE_DONE) {
    goto cleanup;
  }
  sqlite3_finalize(statement);
  statement = NULL;

  if (sqlite3_prepare_v2(db, player_sql, -1, &statement, NULL) != SQLITE_OK ||
      sqlite3_bind_int64(statement, 1, participant->database_player_id) != SQLITE_OK ||
      sqlite3_bind_int64(statement, 2, duration_seconds) != SQLITE_OK ||
      sqlite3_step(statement) != SQLITE_DONE || sqlite3_changes(db) != 1) {
    goto cleanup;
  }
  sqlite3_finalize(statement);
  statement = NULL;

  snprintf(metadata, sizeof(metadata),
           "{\"runtime_player_id\":%u,\"kills\":%u,\"spores\":%u,\"disconnected\":%s}",
           participant->runtime_player_id, participant->round_stats.kills,
           participant->round_stats.spores_collected, participant->disconnected ? "true" : "false");
  if (sqlite3_prepare_v2(db, event_sql, -1, &statement, NULL) != SQLITE_OK ||
      sqlite3_bind_int64(statement, 1, session_id) != SQLITE_OK ||
      sqlite3_bind_int64(statement, 2, (sqlite3_int64)final_tick) != SQLITE_OK ||
      sqlite3_bind_int64(statement, 3, participant->database_player_id) != SQLITE_OK ||
      sqlite3_bind_double(statement, 4, participant->final_mass) != SQLITE_OK ||
      sqlite3_bind_text(statement, 5, metadata, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
      sqlite3_step(statement) != SQLITE_DONE) {
    goto cleanup;
  }
  success = true;

cleanup:
  sqlite3_finalize(statement);
  return success;
}

ShroomMatchPersistenceResult ShroomMatchPersistenceSave(sqlite3* db,
                                                        const ShroomCompletedMatch* match) {
  static const char* const session_sql =
      "INSERT INTO sessions (session_uuid,player_count,bot_count,status,lobby_id,round_id,"
      "game_mode,winner_runtime_player_id) VALUES (?1,?2,?3,'active',?4,?5,?6,?7)";
  static const char* const completed_event_sql =
      "INSERT INTO match_events (session_id,event_type,tick_number,metadata) VALUES "
      "(?1,?2,?3,?4)";
  static const char* const finalize_sql =
      "UPDATE sessions SET ended_at=strftime('%Y-%m-%dT%H:%M:%SZ','now'),duration_seconds=?1,"
      "status=?2,winner_runtime_player_id=?3 WHERE id=?4 AND status='active'";
  sqlite3_stmt* statement = NULL;
  int64_t session_id;
  char metadata[128];

  if ((db == NULL) || (match == NULL) || (match->session_uuid == NULL) ||
      (match->session_uuid[0] == '\0') ||
      ((match->participant_count > 0u) && (match->participants == NULL)) ||
      !Execute(db, "BEGIN IMMEDIATE")) {
    return SHROOM_MATCH_PERSISTENCE_ERROR;
  }
  if (sqlite3_prepare_v2(db, "SELECT id,status FROM sessions WHERE session_uuid=?1", -1,
                         &statement, NULL) != SQLITE_OK ||
      sqlite3_bind_text(statement, 1, match->session_uuid, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
    goto rollback;
  }
  if (sqlite3_step(statement) == SQLITE_ROW) {
    session_id = sqlite3_column_int64(statement, 0);
    if (strcmp((const char*)sqlite3_column_text(statement, 1), "active") != 0) {
      sqlite3_finalize(statement);
      Execute(db, "ROLLBACK");
      return SHROOM_MATCH_PERSISTENCE_ALREADY_SAVED;
    }
    sqlite3_finalize(statement);
    statement = NULL;
  } else {
    sqlite3_finalize(statement);
    statement = NULL;
    if (sqlite3_prepare_v2(db, session_sql, -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, match->session_uuid, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 2, (sqlite3_int64)match->participant_count) != SQLITE_OK ||
        sqlite3_bind_int(statement, 3, match->bot_count) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 4, match->lobby_id) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 5, match->round_id) != SQLITE_OK ||
        sqlite3_bind_int(statement, 6, match->game_mode) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 7, match->winner_runtime_player_id) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_DONE) {
      goto rollback;
    }
    session_id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(statement);
    statement = NULL;
  }

  for (size_t index = 0u; index < match->participant_count; ++index) {
    if (!SaveParticipant(db, session_id, match->duration_seconds, &match->participants[index],
                         match->final_tick)) {
      goto rollback;
    }
  }
  if (sqlite3_prepare_v2(db, finalize_sql, -1, &statement, NULL) != SQLITE_OK ||
      sqlite3_bind_int64(statement, 1, match->duration_seconds) != SQLITE_OK ||
      sqlite3_bind_text(statement, 2, match->interrupted ? "aborted" : "completed", -1,
                        SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_int64(statement, 3, match->winner_runtime_player_id) != SQLITE_OK ||
      sqlite3_bind_int64(statement, 4, session_id) != SQLITE_OK ||
      sqlite3_step(statement) != SQLITE_DONE || sqlite3_changes(db) != 1) {
    goto rollback;
  }
  sqlite3_finalize(statement);
  statement = NULL;
  if (match->interrupted) {
    snprintf(metadata, sizeof(metadata), "{\"reason\":\"graceful_shutdown\"}");
  } else {
    snprintf(metadata, sizeof(metadata), "{\"winner_runtime_player_id\":%u}",
             match->winner_runtime_player_id);
  }
  if (sqlite3_prepare_v2(db, completed_event_sql, -1, &statement, NULL) != SQLITE_OK ||
      sqlite3_bind_int64(statement, 1, session_id) != SQLITE_OK ||
      sqlite3_bind_text(statement, 2, match->interrupted ? "match_interrupted" : "match_completed",
                        -1, SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_int64(statement, 3, (sqlite3_int64)match->final_tick) != SQLITE_OK ||
      sqlite3_bind_text(statement, 4, metadata, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
      sqlite3_step(statement) != SQLITE_DONE) {
    goto rollback;
  }
  sqlite3_finalize(statement);
  if (!Execute(db, "COMMIT")) {
    Execute(db, "ROLLBACK");
    return SHROOM_MATCH_PERSISTENCE_ERROR;
  }
  return SHROOM_MATCH_PERSISTENCE_SAVED;

rollback:
  sqlite3_finalize(statement);
  Execute(db, "ROLLBACK");
  return SHROOM_MATCH_PERSISTENCE_ERROR;
}
