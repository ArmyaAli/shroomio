## Description
Players report being unable to ready up in the lobby. The ready button appears unresponsive or the ready state doesn't persist, preventing matches from starting.

## Steps to Reproduce
1. Join an online lobby
2. Attempt to click the "Ready" button
3. Observe that ready state doesn't change or immediately reverts

## Expected Behavior
- Clicking "Ready" button toggles ready state
- Ready state is visible to other players in the lobby
- Server receives ready state updates
- Match can start when all players are ready

## Current Implementation
The ready state is currently tracked locally in `lobby_roster.c` but:
- No packet is sent to the server when ready state changes
- Server doesn't broadcast player ready states
- Ready state is not validated or synchronized

## Technical Notes
- `g_ready_state` variable exists in `src/client/screens/lobby_roster.c`
- Button handler toggles the local variable
- No network message is sent to inform the server
- Server has no concept of player ready state

## Acceptance Criteria
- [ ] Ready button toggles state and sends update to server
- [ ] Server broadcasts ready state changes to all lobby members
- [ ] Ready state displayed correctly for all players in roster
- [ ] Match start logic checks all players are ready
- [ ] Ready state persists through brief network interruptions

## Priority
Critical - blocks online multiplayer functionality
