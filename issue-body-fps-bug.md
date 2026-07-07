## Description
When a player reaches very large mass (estimated 1000+), the game experiences significant FPS drops and audio stuttering. This affects gameplay experience during late-game scenarios where players have grown substantially.

## Steps to Reproduce
1. Start a game (offline or online)
2. Consume spores and other players to grow to very large mass (1000+)
3. Observe FPS degradation and audio stuttering

## Expected Behavior
- Smooth framerate regardless of player mass
- Clean audio playback without stuttering
- Consistent performance across all mass ranges

## Actual Behavior
- Noticeable FPS drops when mass exceeds ~1000
- Audio begins to stutter and crackle
- Performance degradation appears correlated with player mass/size

## Environment
- Platform: Linux (likely affects all platforms)
- Build: Latest main branch

## Possible Causes
- Rendering overhead from larger player radius (more particles, larger collision checks)
- Audio buffer underruns due to frame time spikes
- Inefficient scaling in rendering or physics calculations
- Memory allocation patterns at large mass values

## Priority
High - affects core gameplay experience during important moments
