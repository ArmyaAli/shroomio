-- Shroomio Database Schema
-- Consolidated schema for pre-production development

-- Enable foreign keys
PRAGMA foreign_keys = ON;

-- ============================================================================
-- Core Game Tables
-- ============================================================================

-- Players table: tracks all known players by their unique identifier
CREATE TABLE IF NOT EXISTS players (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    player_uuid TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    last_seen_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    total_sessions INTEGER NOT NULL DEFAULT 0,
    total_play_time_seconds INTEGER NOT NULL DEFAULT 0
);

-- Sessions table: tracks individual game sessions
CREATE TABLE IF NOT EXISTS sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_uuid TEXT NOT NULL UNIQUE,
    started_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    ended_at TEXT,
    duration_seconds INTEGER,
    player_count INTEGER NOT NULL DEFAULT 0,
    bot_count INTEGER NOT NULL DEFAULT 0,
    lobby_id INTEGER NOT NULL DEFAULT 0,
    round_id INTEGER NOT NULL DEFAULT 0,
    game_mode INTEGER NOT NULL DEFAULT 0,
    winner_runtime_player_id INTEGER NOT NULL DEFAULT 0,
    status TEXT NOT NULL CHECK (status IN ('active', 'completed', 'aborted')) DEFAULT 'active'
);

-- Stable identities prevent cached chat from crossing lobby/database lifetimes.
CREATE TABLE IF NOT EXISTS lobby_history_identities (
    lobby_id INTEGER PRIMARY KEY,
    history_identity INTEGER NOT NULL UNIQUE CHECK (history_identity > 0)
);

CREATE TABLE IF NOT EXISTS chat_history (
    history_identity INTEGER NOT NULL,
    message_id INTEGER NOT NULL,
    sender_id INTEGER NOT NULL,
    timestamp_sec INTEGER NOT NULL,
    sender_name TEXT NOT NULL,
    message TEXT NOT NULL,
    PRIMARY KEY(history_identity, message_id)
);

-- Session participants: tracks which players participated in each session
CREATE TABLE IF NOT EXISTS session_participants (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER NOT NULL,
    player_id INTEGER NOT NULL,
    runtime_player_id INTEGER NOT NULL DEFAULT 0,
    joined_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    left_at TEXT,
    final_rank INTEGER,
    final_mass REAL,
    disconnected INTEGER NOT NULL DEFAULT 0 CHECK (disconnected IN (0, 1)),
    kills INTEGER NOT NULL DEFAULT 0,
    deaths INTEGER NOT NULL DEFAULT 0,
    mass_consumed REAL NOT NULL DEFAULT 0.0,
    mass_lost REAL NOT NULL DEFAULT 0.0,
    distance_traveled REAL NOT NULL DEFAULT 0.0,
    spores_collected INTEGER NOT NULL DEFAULT 0,
    powerups_collected INTEGER NOT NULL DEFAULT 0,
    peak_mass REAL NOT NULL DEFAULT 0.0,
    center_zone_seconds REAL NOT NULL DEFAULT 0.0,
    mid_zone_seconds REAL NOT NULL DEFAULT 0.0,
    outer_zone_seconds REAL NOT NULL DEFAULT 0.0,
    splits_used INTEGER NOT NULL DEFAULT 0,
    ejects_used INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE,
    FOREIGN KEY (player_id) REFERENCES players(id) ON DELETE CASCADE,
    UNIQUE(session_id, player_id)
);

-- Player statistics: tracks cumulative stats per player
CREATE TABLE IF NOT EXISTS player_stats (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    player_id INTEGER NOT NULL UNIQUE,
    total_games_played INTEGER NOT NULL DEFAULT 0,
    total_kills INTEGER NOT NULL DEFAULT 0,
    total_deaths INTEGER NOT NULL DEFAULT 0,
    total_mass_consumed REAL NOT NULL DEFAULT 0.0,
    total_mass_lost REAL NOT NULL DEFAULT 0.0,
    total_distance_traveled REAL NOT NULL DEFAULT 0.0,
    total_spores_consumed INTEGER NOT NULL DEFAULT 0,
    total_players_consumed INTEGER NOT NULL DEFAULT 0,
    highest_mass_achieved REAL NOT NULL DEFAULT 0.0,
    highest_rank_achieved INTEGER NOT NULL DEFAULT 999,
    longest_survival_seconds REAL NOT NULL DEFAULT 0.0,
    updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    FOREIGN KEY (player_id) REFERENCES players(id) ON DELETE CASCADE
);

-- Match events: tracks significant events during gameplay
CREATE TABLE IF NOT EXISTS match_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER NOT NULL,
    event_type TEXT NOT NULL CHECK (event_type IN (
        'player_spawn',
        'player_death',
        'player_consume_player',
        'player_consume_spore',
        'player_reach_mass',
        'game_tick',
        'participant_summary',
        'match_completed',
        'match_interrupted'
    )),
    event_timestamp TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    tick_number INTEGER,
    actor_player_id INTEGER,
    target_player_id INTEGER,
    mass_value REAL,
    position_x REAL,
    position_y REAL,
    metadata TEXT,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE,
    FOREIGN KEY (actor_player_id) REFERENCES players(id) ON DELETE SET NULL,
    FOREIGN KEY (target_player_id) REFERENCES players(id) ON DELETE SET NULL
);

-- ============================================================================
-- Authentication Tables
-- ============================================================================

-- Users table: authentication accounts linked to players
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    player_id INTEGER NOT NULL UNIQUE,
    username TEXT NOT NULL UNIQUE,
    email TEXT COLLATE NOCASE UNIQUE,
    password_hash TEXT,
    discord_id TEXT UNIQUE,
    auth_method TEXT NOT NULL CHECK (auth_method IN ('password', 'discord', 'anonymous')),
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    last_login_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    FOREIGN KEY (player_id) REFERENCES players(id) ON DELETE CASCADE
);

-- Auth tokens table: active session tokens for authenticated users
CREATE TABLE IF NOT EXISTS auth_tokens (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    token TEXT UNIQUE,
    token_hash TEXT UNIQUE,
    token_type TEXT NOT NULL DEFAULT 'jwt',
    token_kind TEXT NOT NULL DEFAULT 'legacy'
        CHECK (token_kind IN ('legacy', 'access', 'refresh')),
    refresh_family_id TEXT,
    expires_at TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    revoked_at TEXT,
    rotated_at TEXT,
    replaced_by_hash TEXT,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- Mushroom species catalog: cosmetic species metadata delivered to clients
CREATE TABLE IF NOT EXISTS mushroom_species (
    species_id INTEGER PRIMARY KEY CHECK (species_id >= 0 AND species_id < 10),
    name TEXT NOT NULL,
    description TEXT NOT NULL,
    pattern_id INTEGER NOT NULL DEFAULT 0,
    rarity_tier INTEGER NOT NULL DEFAULT 0,
    cap_color_rgba INTEGER NOT NULL DEFAULT 0,
    unlocked_by_default INTEGER NOT NULL DEFAULT 1 CHECK (unlocked_by_default IN (0, 1)),
    sort_order INTEGER NOT NULL UNIQUE
);

-- ============================================================================
-- Views
-- ============================================================================

-- Leaderboard view: shows top players by various metrics
CREATE VIEW IF NOT EXISTS leaderboard_by_mass AS
SELECT 
    p.display_name,
    ps.highest_mass_achieved,
    ps.total_games_played,
    ps.total_kills,
    p.last_seen_at
FROM players p
JOIN player_stats ps ON p.id = ps.player_id
WHERE ps.highest_mass_achieved > 0
ORDER BY ps.highest_mass_achieved DESC;

CREATE VIEW IF NOT EXISTS leaderboard_by_kills AS
SELECT 
    p.display_name,
    ps.total_kills,
    ps.total_players_consumed,
    ps.total_games_played,
    p.last_seen_at
FROM players p
JOIN player_stats ps ON p.id = ps.player_id
WHERE ps.total_kills > 0
ORDER BY ps.total_kills DESC;

-- ============================================================================
-- Indexes
-- ============================================================================

-- Core game indexes
CREATE INDEX IF NOT EXISTS idx_players_uuid ON players(player_uuid);
CREATE INDEX IF NOT EXISTS idx_players_last_seen ON players(last_seen_at DESC);
CREATE INDEX IF NOT EXISTS idx_sessions_started ON sessions(started_at DESC);
CREATE INDEX IF NOT EXISTS idx_sessions_status ON sessions(status);
CREATE INDEX IF NOT EXISTS idx_session_participants_session ON session_participants(session_id);
CREATE INDEX IF NOT EXISTS idx_session_participants_player ON session_participants(player_id);
CREATE INDEX IF NOT EXISTS idx_player_stats_player ON player_stats(player_id);
CREATE INDEX IF NOT EXISTS idx_match_events_session ON match_events(session_id);
CREATE INDEX IF NOT EXISTS idx_match_events_type ON match_events(event_type);
CREATE INDEX IF NOT EXISTS idx_match_events_timestamp ON match_events(event_timestamp DESC);

-- Authentication indexes
CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
CREATE INDEX IF NOT EXISTS idx_users_email ON users(email COLLATE NOCASE);
CREATE INDEX IF NOT EXISTS idx_users_discord_id ON users(discord_id);
CREATE INDEX IF NOT EXISTS idx_users_player_id ON users(player_id);
CREATE INDEX IF NOT EXISTS idx_auth_tokens_token ON auth_tokens(token);
CREATE INDEX IF NOT EXISTS idx_auth_tokens_hash ON auth_tokens(token_hash);
CREATE INDEX IF NOT EXISTS idx_auth_tokens_family ON auth_tokens(refresh_family_id);
CREATE INDEX IF NOT EXISTS idx_auth_tokens_user_id ON auth_tokens(user_id);
CREATE INDEX IF NOT EXISTS idx_auth_tokens_expires_at ON auth_tokens(expires_at);
CREATE INDEX IF NOT EXISTS idx_mushroom_species_sort_order ON mushroom_species(sort_order);
