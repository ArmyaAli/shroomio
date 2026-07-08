## Summary
Implements a procedurally generated ambient music track that plays during gameplay, addressing issue #252.

## Changes
- **Enhanced music generation**: Replaced the simple 3.2-second ambient loop with a 48-second immersive track
- **Musical structure**: Features a peaceful C-G-Am-F chord progression in C major at 60 BPM
- **Layered instrumentation**:
  - Bass drone (root note, one octave down)
  - Chord pad (root, major third, perfect fifth)
  - Gentle arpeggiated melody (alternating root and fifth)
  - Subtle shimmer/high harmonics
- **Smooth transitions**: Fade in/out at loop boundaries and between chords
- **Soft clipping**: Uses tanh() to avoid harsh artifacts

## Technical Details
- Music is generated at runtime using sine waves and mathematical functions
- No external audio files required
- 48-second loop provides variety without repetition fatigue
- Respects existing music volume and master volume settings
- Automatically starts/stops based on volume settings

## Acceptance Criteria Met
- [x] Gentle looping background music track
- [x] Music respects existing music volume and master volume settings
- [x] Music starts/stops cleanly across menu and gameplay transitions
- [x] No obvious clicks, harsh cuts, or distracting repetition
- [x] Default volume is subtle enough to preserve a calm experience

## Testing
- All 19 unit tests pass
- All 34 ImGui UI tests pass
- Client builds cleanly on Linux with no warnings

Closes #252
