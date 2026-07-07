## Description
The match timer was implemented server-side (issue #382) but is not displayed in the client HUD. Players cannot see the countdown or know when the match will end.

## Current State
- Server sends `match_time_remaining` in snapshot packets
- Server sends `match_phase` (RUNNING/RESULTS/RESET) in snapshot packets  
- Server sends podium data (top 3 players) in snapshot packets
- Client receives all this data but doesn't render it

## Expected Behavior
- Match timer displayed prominently in HUD (top center or top right)
- Timer shows MM:SS format
- Timer changes color when < 60 seconds (yellow) and < 10 seconds (red)
- During RESULTS phase, show podium with top 3 players and their masses
- During RESET phase, show "Match restarting..." message

## Acceptance Criteria
- [ ] Match timer visible in HUD during online matches
- [ ] Timer counts down in real-time
- [ ] Timer color changes based on remaining time
- [ ] Podium displayed during RESULTS phase
- [ ] Reset message shown during RESET phase
- [ ] Timer hidden in offline mode

## Priority
High - core gameplay feature that affects match pacing and strategy
