## Summary
Adds foundational infrastructure for multiple game modes as requested in #216.

## Changes
- **Game Mode Enum**: Added `ShroomGameMode` enum with FFA, Teams (2v2/3v3/4v4), Battle Royale, King of Hill, and Mass Race modes
- **Team Support**: Added `team_id` field to `ShroomPlayerState` for team-based modes
- **World State**: Added `game_mode`, `team_count`, and `team_masses` fields to `ShroomWorldState`
- **Protocol Updates**: Updated `ShroomSnapshotPlayerState` and `ShroomSnapshotPacket` to include game mode and team information
- **UI**: Created new `game_mode_select.c` screen with mode selection interface
- **Main Menu**: Added "Game Modes" button to main menu that transitions to mode selection
- **Screen Manager**: Added `SHROOM_SCREEN_GAME_MODE_SELECT` screen type

## What's Included
- Game mode infrastructure and data structures
- Mode selection UI with all planned modes
- Protocol updates to support game modes and teams
- Updated tests for new packet sizes

## What's Next (Future PRs)
- Team assignment logic
- Team-based scoring and win conditions
- Battle Royale shrinking zone mechanics
- King of the Hill zone control logic
- Mass Race win condition
- Server-side game mode handling
- Mode-specific HUD elements

## Acceptance Criteria (Partial)
- [x] Mode selection UI is intuitive
- [x] Game mode system is modular and extensible
- [x] All existing tests pass
- [x] Client builds cleanly

This PR provides the foundation for implementing the actual game mode logic in follow-up PRs.

Closes #216 (partial - infrastructure only)
