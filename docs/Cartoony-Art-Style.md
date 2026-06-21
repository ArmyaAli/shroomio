# Cartoony Art Style Guide

This guide defines the visual direction for shroomio's cartoony fungal identity. Use it when changing menus, HUD, arena art, particles, website screenshots, or promotional assets.

## Art Pillars

- **Soft fungal shapes:** prefer rounded caps, bulbous stems, spores, rings, and organic blobs over hard rectangles.
- **Readable at speed:** gameplay silhouettes and HUD text must stay clear when players move quickly or overlap.
- **Playful but competitive:** keep forms cute and saturated, but preserve strong contrast for threats, warnings, and leaderboard information.
- **Handmade texture:** use subtle speckles, cap spots, gills, moss, and spore dust instead of flat sterile panels.

## Palette

| Role | Color | Hex | Usage |
|---|---:|---|---|
| Night mycelium | `18, 18, 26` | `#12121A` | base background |
| Deep cap brown | `62, 42, 33` | `#3E2A21` | panels, dark outlines |
| Warm stem tan | `224, 184, 128` | `#E0B880` | primary UI fills |
| Spore gold | `255, 228, 112` | `#FFE470` | collectibles and positive feedback |
| Moss green | `112, 224, 128` | `#70E080` | success, safe growth |
| Amanita red | `245, 84, 84` | `#F55454` | danger, death, errors |
| Glow purple | `186, 104, 200` | `#BA68C8` | rare powerups, highlights |
| Dew blue | `118, 210, 255` | `#76D2FF` | neutral info, movement trails |

Contrast rules:

- Body text must meet at least 4.5:1 contrast against its panel.
- Warning text and icons must remain distinguishable without relying on hue alone; pair red/orange with icons, outlines, or motion.
- Keep alpha-backed panels at 70% opacity or higher when text is inside them.

## Shape Language

- Buttons use pill or mushroom-cap silhouettes with 10-18 px corner radii at 720p.
- Panels use thick dark outlines, soft inner highlights, and small cap-spot accents near corners.
- HUD meters should look like growing stems, hyphae, or spore sacks rather than generic bars.
- Minimap and proximity widgets should use circular or scalloped frames.
- Avoid sharp 90-degree borders except for debug overlays and tabular diagnostic data.

## Characters And Arena

- Player colonies read as mushroom caps from above: round cap body, lighter rim, cap spots, and a short movement squash.
- Larger colonies gain richer cap spots and a thicker outline; smaller colonies stay simple to avoid visual noise.
- Split pieces keep the same palette but use smaller cap spots and a short gold spore burst at creation.
- Spores are warm gold dots with a tiny halo. Powerups use larger silhouettes with distinct inner symbols.
- Arena zones should differ by texture density: sparse outer moss, medium mycelium strands, dense glowing center spores.

## UI Components

- **Main menu:** one large fungal panel, spore-dust background, oversized title, stacked cap-shaped buttons.
- **Settings:** same panel treatment as the menu; sliders should have stem tracks and spore knobs.
- **HUD:** keep mass, rank, latency, and zone text compact. Use earthy panels with bright event colors only for changing state.
- **Notifications:** slide/fade like spores drifting upward; use title color to communicate severity.
- **Chat and lobby panels:** prioritize readability; use the theme in borders, headings, and selected rows rather than heavy textures.

## Animation

- Use short organic easing: 100-180 ms for button press, 180-300 ms for panel entrance, 300-500 ms for zone callouts.
- Button press should squash down by 2-4 px and rebound once.
- Hover states should glow or brighten, not shake.
- Gameplay particles should expand quickly, drift outward, and fade softly.
- Looping background motion must remain subtle enough to avoid distracting from menus.

## Asset Rules

- Place source-ready art in `assets/` by type: `assets/sprites/`, `assets/audio/`, `assets/ui/`, and `assets/marketing/`.
- Name files by purpose and state, for example `button_cap_idle.png`, `button_cap_pressed.png`, `powerup_speed_icon.png`.
- Prefer power-of-two texture atlases for gameplay/UI sprites when multiple related sprites are loaded together.
- Keep original editable sources beside exports with a clear suffix, for example `main_menu_background.source.svg`.
- New raster assets should be tested at 1280x720 and 1920x1080 before landing.

## Implementation Checklist

- Does the change use the palette or a documented extension of it?
- Are silhouettes readable at gameplay scale?
- Are text and critical indicators accessible by contrast and shape?
- Does the animation support feedback without delaying input?
- Are asset names and locations consistent with this guide?
- Did the change avoid adding large textures where a shape, gradient, or generated effect would work?
