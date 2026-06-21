# Assets

Tracked art and audio assets live here. Runtime code may still generate simple effects procedurally, but exported assets should follow the cartoony fungal style guide in `docs/Cartoony-Art-Style.md`.

## Layout

- `sprites/` - gameplay sprites, UI icons, texture atlases, and editable source art.
- `audio/` - exported music loops, sound effects, and editable source project notes.
- `maps/` - future arena layouts or handcrafted spawn/decoration data.
- `shaders/` - future shader sources for post-processing or stylized effects.

## Naming

- Use lowercase purpose-first names: `button_cap_idle.png`, `powerup_speed_icon.svg`, `spore_pop_01.wav`.
- Keep editable source files beside exports using `.source` before the extension.
- Prefer SVG for simple UI motifs and small PNG atlases for gameplay sprites.
- Keep new assets small enough for fast CI checkouts unless there is a clear gameplay need.

## Current Themed Content

- `sprites/shroomio-fungal-icons.source.svg` defines reusable fungal motifs for menu buttons, spores, powerups, and warning/error states.
