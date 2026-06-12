# Shroomio Database

This directory contains the database schema for persistent storage in the shroomio server.

## Overview

The shroomio server uses SQLite for persistent storage of:
- Player statistics and history
- Game session records
- Match events for analytics
- User authentication and session tokens

## Directory Structure

```
database/
├── schema.sql    # Consolidated schema definition
└── README.md
```

## Schema

The database consists of seven core tables:

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

### users
Authentication accounts linked to players. Supports password, Discord OAuth, and anonymous authentication methods.

### auth_tokens
Active session tokens for authenticated users with expiration tracking.

## Database Schema

The schema is maintained in a single `schema.sql` file. Since we're pre-production, we maintain one consolidated schema file that represents the current database state. As features are added, the schema file is updated directly rather than using incremental migrations.

## Database Location

By default, the SQLite database file is created at `data/shroomio.db` relative to the server working directory.

## Development

To initialize the database for development:

```bash
sqlite3 data/shroomio.db < database/schema.sql
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
