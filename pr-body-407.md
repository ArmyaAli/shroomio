## Summary
Implements proper ready state protocol for online lobbies so players can actually ready up before entering matches.

## Changes
- **Protocol**: Added `SHROOM_PACKET_READY_STATE` packet type with player_id and is_ready fields
- **Client**: Added `ClientNetSendReadyState()` function that sends ready state changes to server
- **Server**: Added `HandleReadyStatePacket()` to receive and track ready state per session
- **Server**: Added `is_ready` field to `ServerSession` struct to track player ready state
- **UI**: Ready button in lobby roster now sends state to server when clicked
- **Logging**: Server logs ready state changes for debugging

## Problem
Previously, the ready button in the lobby roster only toggled a local UI state (`g_ready_state`) without sending anything to the server. This meant:
- Other players could not see who was ready
- The server had no way to track ready states
- Ready state was purely cosmetic with no actual effect

## Solution
Implemented a proper client-server protocol for ready state:
1. Client sends `READY_STATE` packet when ready button is clicked
2. Server receives packet and updates session's `is_ready` field
3. Server logs the state change for debugging
4. Ready state is now tracked server-side and can be used for match start logic

## Acceptance Criteria
- [x] Ready button sends state to server
- [x] Server tracks ready state per session
- [x] Server logs ready state changes
- [x] All existing tests pass
- [x] Client and server build cleanly

Closes #407
