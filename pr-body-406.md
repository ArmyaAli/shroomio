## Summary
Displays the match timer in the client HUD during online matches, showing countdown and color-coded urgency.

## Changes
- **Protocol**: Parse match timer fields from snapshot packets (match_phase, match_time_remaining, podium data)
- **Client State**: Added match timer fields to ClientNetState struct
- **HUD Display**: Added match timer panel at top-center of screen
- **Color Coding**: Timer changes color based on time remaining:
  - White: > 60 seconds remaining
  - Yellow/Orange: < 60 seconds remaining  
  - Red: < 10 seconds remaining
- **Visibility**: Timer only shown in online mode after welcome received
- **Format**: Displays time as MM:SS

## Implementation Details
The server already sends match timer data in snapshot packets (from #382), but the client wasn't parsing or displaying it. This PR:

1. Parses `match_phase`, `match_time_remaining`, and podium data from incoming snapshots
2. Stores these values in `ClientNetState` for HUD access
3. Renders a fungal-themed HUD panel at top-center showing the countdown
4. Uses color coding to indicate urgency as time runs out
5. Only displays during online matches (hidden in offline/practice modes)

## Acceptance Criteria
- [x] Match timer visible in HUD during online matches
- [x] Timer shows MM:SS format
- [x] Timer color changes based on remaining time
- [x] Timer hidden in offline mode
- [x] All existing tests pass
- [x] Client builds cleanly

Closes #406
