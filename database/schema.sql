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
    status TEXT NOT NULL CHECK (status IN ('active', 'completed', 'aborted')) DEFAULT 'active'
);

-- Session participants: tracks which players participated in each session
CREATE TABLE IF NOT EXISTS session_participants (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER NOT NULL,
    player_id INTEGER NOT NULL,
    joined_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    left_at TEXT,
    final_rank INTEGER,
    final_mass REAL,
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
        'game_tick'
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
    email TEXT,
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
    token TEXT NOT NULL UNIQUE,
    token_type TEXT NOT NULL DEFAULT 'jwt',
    expires_at TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    revoked_at TEXT,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
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
CREATE INDEX IF NOT EXISTS idx_users_discord_id ON users(discord_id);
CREATE INDEX IF NOT EXISTS idx_users_player_id ON users(player_id);
CREATE INDEX IF NOT EXISTS idx_auth_tokens_token ON auth_tokens(token);
CREATE INDEX IF NOT EXISTS idx_auth_tokens_user_id ON auth_tokens(user_id);
CREATE INDEX IF NOT EXISTS idx_auth_tokens_expires_at ON auth_tokens(expires_at);
