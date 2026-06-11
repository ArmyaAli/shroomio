# Shroomio Database

This directory contains the database schema and migrations for persistent storage in the shroomio server.

## Overview

The shroomio server uses SQLite for persistent storage of:
- Player statistics and history
- Game session records
- Match events for analytics

## Directory Structure

```
database/
├── migrations/
│   ├── 001_initial_schema.sql    # Core schema definition
│   └── 001_seed_data.sql         # Sample data for development
└── README.md
```

## Schema

The database consists of five core tables:

### players
Tracks all known players by UUID and display name. Updated on each session participation.

### sessions
Records individual game sessions with timing, participant counts, and status (active/completed/aborted).

### session_participants
Links players to sessions with join/leave times and final stats (rank, mass).

### player_stats
Cumulative statistics per player: games played, kills, deaths, mass consumed, distance traveled, etc.

### match_events
Significant in-game events (spawns, deaths, consumptions) for analytics and replay purposes.

## Migrations

Migration files are applied in lexicographic order on server startup. The naming convention is:

```
NNN_description.sql
```

Where `NNN` is a zero-padded migration number (e.g., `001`, `002`).

## Database Location

By default, the SQLite database file is created at `data/shroomio.db` relative to the server working directory.

## Development

To initialize the database with seed data for development:

```bash
sqlite3 data/shroomio.db < database/migrations/001_initial_schema.sql
sqlite3 data/shroomio.db < database/migrations/001_seed_data.sql
```

## Views

Two leaderboard views are provided for convenience:
- `leaderboard_by_mass` - Top players by highest mass achieved
- `leaderboard_by_kills` - Top players by total kills

## Dependencies

The server requires the SQLite3 development library:
- Ubuntu/Debian: `libsqlite3-dev`
- macOS: `sqlite3` (included with Xcode)
- Alpine: `sqlite-dev`
