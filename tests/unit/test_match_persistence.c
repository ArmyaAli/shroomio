#include "unity.h"

#include <sqlite3.h>

#include "server/database.h"
#include "server/match_persistence.h"

static sqlite3* db;

static int64_t ScalarInt64(const char* sql) {
  sqlite3_stmt* statement = NULL;
  int64_t value = -1;

  TEST_ASSERT_EQUAL(SQLITE_OK, sqlite3_prepare_v2(db, sql, -1, &statement, NULL));
  TEST_ASSERT_EQUAL(SQLITE_ROW, sqlite3_step(statement));
  value = sqlite3_column_int64(statement, 0);
  sqlite3_finalize(statement);
  return value;
}

void setUp(void) {
  TEST_ASSERT_EQUAL(SQLITE_OK, sqlite3_open(":memory:", &db));
  TEST_ASSERT_TRUE(ShroomDatabaseInitializeSchema(db));
  TEST_ASSERT_EQUAL(
      SQLITE_OK,
      sqlite3_exec(db,
                   "INSERT INTO players (id,player_uuid,display_name) VALUES "
                   "(1,'player-one','One'),(2,'player-two','Two')",
                   NULL, NULL, NULL));
}

void tearDown(void) {
  sqlite3_close(db);
  db = NULL;
}

static ShroomCompletedMatch MakeMatch(const char* uuid,
                                      const ShroomPersistedParticipant* participants,
                                      size_t participant_count) {
  return (ShroomCompletedMatch){
      .session_uuid = uuid,
      .lobby_id = 7u,
      .round_id = 3u,
      .game_mode = SHROOM_GAME_MODE_FFA,
      .final_tick = 900u,
      .duration_seconds = 30u,
      .bot_count = 4u,
      .winner_runtime_player_id = 101u,
      .participants = participants,
      .participant_count = participant_count,
  };
}

static void test_completed_match_persists_session_participants_events_and_lifetime_stats(void) {
  const ShroomPersistedParticipant participants[] = {
      {.database_player_id = 1,
       .runtime_player_id = 101u,
       .final_rank = 1,
       .final_mass = 480.0f,
       .round_stats = {.peak_mass = 640.0f, .kills = 3u, .spores_collected = 19u}},
      {.database_player_id = 2,
       .runtime_player_id = 202u,
       .final_rank = 2,
       .final_mass = 250.0f,
       .round_stats = {.peak_mass = 300.0f, .kills = 1u, .spores_collected = 8u}},
  };
  const ShroomCompletedMatch match = MakeMatch("round-complete", participants, 2u);

  TEST_ASSERT_EQUAL(SHROOM_MATCH_PERSISTENCE_SAVED, ShroomMatchPersistenceSave(db, &match));
  TEST_ASSERT_EQUAL_INT64(1, ScalarInt64("SELECT count(*) FROM sessions WHERE status='completed'"));
  TEST_ASSERT_EQUAL_INT64(2, ScalarInt64("SELECT count(*) FROM session_participants"));
  TEST_ASSERT_EQUAL_INT64(3, ScalarInt64("SELECT count(*) FROM match_events"));
  TEST_ASSERT_EQUAL_INT64(3, ScalarInt64("SELECT total_kills FROM player_stats WHERE player_id=1"));
  TEST_ASSERT_EQUAL_INT64(
      19, ScalarInt64("SELECT total_spores_consumed FROM player_stats WHERE player_id=1"));
  TEST_ASSERT_EQUAL_INT64(1, ScalarInt64("SELECT total_sessions FROM players WHERE id=1"));
}

static void test_begin_creates_active_session_with_stable_uuid(void) {
  const ShroomCompletedMatch match = MakeMatch("123e4567-e89b-12d3-a456-426614174000", NULL, 0u);

  TEST_ASSERT_TRUE(ShroomMatchPersistenceBegin(db, &match));
  TEST_ASSERT_EQUAL_INT64(1, ScalarInt64("SELECT count(*) FROM sessions WHERE status='active'"));
  TEST_ASSERT_EQUAL_INT64(1, ScalarInt64("SELECT count(*) FROM sessions WHERE session_uuid='123e4567-e89b-12d3-a456-426614174000'"));
  TEST_ASSERT_TRUE(ShroomMatchPersistenceBegin(db, &match));
  TEST_ASSERT_EQUAL_INT64(1, ScalarInt64("SELECT count(*) FROM sessions"));
  TEST_ASSERT_TRUE(ShroomMatchPersistenceRecordEvent(
      db, match.session_uuid, "player_spawn", 12u, 1, 0, 100.0f, 10.0f, 20.0f, "{}"));
  TEST_ASSERT_EQUAL_INT64(1, ScalarInt64("SELECT count(*) FROM match_events"));
}

static void test_duplicate_completion_is_idempotent(void) {
  const ShroomPersistedParticipant participant = {
      .database_player_id = 1,
      .runtime_player_id = 101u,
      .final_rank = 1,
      .final_mass = 300.0f,
      .round_stats = {.peak_mass = 350.0f, .kills = 2u, .spores_collected = 7u},
  };
  const ShroomCompletedMatch match = MakeMatch("round-idempotent", &participant, 1u);

  TEST_ASSERT_EQUAL(SHROOM_MATCH_PERSISTENCE_SAVED, ShroomMatchPersistenceSave(db, &match));
  TEST_ASSERT_EQUAL(SHROOM_MATCH_PERSISTENCE_ALREADY_SAVED,
                    ShroomMatchPersistenceSave(db, &match));
  TEST_ASSERT_EQUAL_INT64(1, ScalarInt64("SELECT total_games_played FROM player_stats WHERE player_id=1"));
  TEST_ASSERT_EQUAL_INT64(2, ScalarInt64("SELECT total_kills FROM player_stats WHERE player_id=1"));
  TEST_ASSERT_EQUAL_INT64(1, ScalarInt64("SELECT count(*) FROM sessions"));
}

static void test_failure_rolls_back_session_and_prior_participant_updates(void) {
  const ShroomPersistedParticipant participants[] = {
      {.database_player_id = 1, .runtime_player_id = 101u, .final_rank = 1},
      {.database_player_id = 999, .runtime_player_id = 999u, .final_rank = 2},
  };
  const ShroomCompletedMatch match = MakeMatch("round-rollback", participants, 2u);

  TEST_ASSERT_EQUAL(SHROOM_MATCH_PERSISTENCE_ERROR, ShroomMatchPersistenceSave(db, &match));
  TEST_ASSERT_EQUAL_INT64(0, ScalarInt64("SELECT count(*) FROM sessions"));
  TEST_ASSERT_EQUAL_INT64(0, ScalarInt64("SELECT count(*) FROM session_participants"));
  TEST_ASSERT_EQUAL_INT64(0, ScalarInt64("SELECT count(*) FROM player_stats"));
  TEST_ASSERT_EQUAL_INT64(0, ScalarInt64("SELECT total_sessions FROM players WHERE id=1"));
}

static void test_disconnect_and_split_colony_summary_are_preserved(void) {
  const ShroomPersistedParticipant participant = {
      .database_player_id = 1,
      .runtime_player_id = 101u,
      .disconnected = true,
      .final_rank = 3,
      .final_mass = 725.0f,
      .round_stats = {
          .peak_mass = 810.0f,
          .kills = 4u,
          .spores_collected = 31u,
          .splits_used = 3u,
          .ejects_used = 2u,
      },
  };
  const ShroomCompletedMatch match = MakeMatch("round-disconnect-split", &participant, 1u);

  TEST_ASSERT_EQUAL(SHROOM_MATCH_PERSISTENCE_SAVED, ShroomMatchPersistenceSave(db, &match));
  TEST_ASSERT_EQUAL_INT64(1, ScalarInt64("SELECT disconnected FROM session_participants"));
  TEST_ASSERT_EQUAL_INT64(4, ScalarInt64("SELECT kills FROM session_participants"));
  TEST_ASSERT_EQUAL_INT64(31, ScalarInt64("SELECT spores_collected FROM session_participants"));
  TEST_ASSERT_EQUAL_INT64(3, ScalarInt64("SELECT splits_used FROM session_participants"));
  TEST_ASSERT_EQUAL_INT64(725, ScalarInt64("SELECT CAST(final_mass AS INTEGER) FROM session_participants"));
}

static void test_interrupted_match_persists_summary_and_lifetime_stats_once(void) {
  const ShroomPersistedParticipant participant = {
      .database_player_id = 1,
      .runtime_player_id = 101u,
      .final_rank = 2,
      .final_mass = 275.0f,
      .round_stats = {.peak_mass = 410.0f, .kills = 2u, .spores_collected = 11u},
  };
  ShroomCompletedMatch match = MakeMatch("round-interrupted", &participant, 1u);

  match.interrupted = true;
  TEST_ASSERT_EQUAL(SHROOM_MATCH_PERSISTENCE_SAVED, ShroomMatchPersistenceSave(db, &match));
  TEST_ASSERT_EQUAL(SHROOM_MATCH_PERSISTENCE_ALREADY_SAVED,
                    ShroomMatchPersistenceSave(db, &match));
  TEST_ASSERT_EQUAL_INT64(1, ScalarInt64("SELECT count(*) FROM sessions WHERE status='aborted'"));
  TEST_ASSERT_EQUAL_INT64(
      1, ScalarInt64("SELECT count(*) FROM match_events WHERE event_type='match_interrupted'"));
  TEST_ASSERT_EQUAL_INT64(
      1, ScalarInt64("SELECT count(*) FROM match_events WHERE event_type='participant_summary'"));
  TEST_ASSERT_EQUAL_INT64(1, ScalarInt64("SELECT total_games_played FROM player_stats WHERE player_id=1"));
  TEST_ASSERT_EQUAL_INT64(2, ScalarInt64("SELECT total_kills FROM player_stats WHERE player_id=1"));
  TEST_ASSERT_EQUAL_INT64(1, ScalarInt64("SELECT total_sessions FROM players WHERE id=1"));
}

static void test_interrupted_match_failure_rolls_back_every_row(void) {
  const ShroomPersistedParticipant participants[] = {
      {.database_player_id = 1, .runtime_player_id = 101u, .final_rank = 1},
      {.database_player_id = 999, .runtime_player_id = 999u, .final_rank = 2},
  };
  ShroomCompletedMatch match = MakeMatch("round-interrupted-rollback", participants, 2u);

  match.interrupted = true;
  TEST_ASSERT_EQUAL(SHROOM_MATCH_PERSISTENCE_ERROR, ShroomMatchPersistenceSave(db, &match));
  TEST_ASSERT_EQUAL_INT64(0, ScalarInt64("SELECT count(*) FROM sessions"));
  TEST_ASSERT_EQUAL_INT64(0, ScalarInt64("SELECT count(*) FROM session_participants"));
  TEST_ASSERT_EQUAL_INT64(0, ScalarInt64("SELECT count(*) FROM match_events"));
  TEST_ASSERT_EQUAL_INT64(0, ScalarInt64("SELECT count(*) FROM player_stats"));
  TEST_ASSERT_EQUAL_INT64(0, ScalarInt64("SELECT total_sessions FROM players WHERE id=1"));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_completed_match_persists_session_participants_events_and_lifetime_stats);
  RUN_TEST(test_begin_creates_active_session_with_stable_uuid);
  RUN_TEST(test_duplicate_completion_is_idempotent);
  RUN_TEST(test_failure_rolls_back_session_and_prior_participant_updates);
  RUN_TEST(test_disconnect_and_split_colony_summary_are_preserved);
  RUN_TEST(test_interrupted_match_persists_summary_and_lifetime_stats_once);
  RUN_TEST(test_interrupted_match_failure_rolls_back_every_row);
  return UNITY_END();
}
