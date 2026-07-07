## Summary
Implements server-side match timer with automatic phase transitions and match reset.

## Changes
- **Match timer countdown**: Timer decrements each tick, transitions to RESULTS phase at 0
- **Podium computation**: Top 3 players by mass with stable tiebreak by player_id
- **Match reset**: After 30s results phase, all players reset to default mass at safe outer spawns
- **Protocol extension**: Snapshot packets now include match_phase, time_remaining, and podium data
- **Server CLI**: Added `--match-duration` flag (default 600s)
- **Tests**: 10 unit tests covering timer countdown, phase transitions, podium sorting, and reset behavior

## Acceptance Criteria
- [x] Server-side match timer (default 600s, configurable via `SHROOM_MATCH_DURATION_SECONDS` and `--match-duration`)
- [x] Timer reaches zero: transitions to RESULTS phase with podium (top-3 by mass)
- [x] After 30s grace period: world resets, all players return to default mass at safe outer spawns
- [x] New `ShroomMatchPhase` enum: RUNNING, RESULTS, RESET
- [x] Tests cover: timer countdown, transition fires at 0, podium sorted by mass with stable tiebreak, reset returns players to default mass, post-reset timer restarts
- [x] Existing tests stay green; format-check + cppcheck clean

Closes #382
