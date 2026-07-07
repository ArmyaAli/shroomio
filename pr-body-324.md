## Summary
Redesigned the GUI theme to use a cohesive fungal color palette that matches the art style, replacing the stock ImGui dark theme.

## Changes
- **Custom fungal palette**: Replaced `ImGui::StyleColorsDark()` base with custom colors derived from `docs/Cartoony-Art-Style.md`
- **Night mycelium backgrounds**: Window/child/popup backgrounds use the `#12121A` base color
- **Deep cap brown borders**: All borders, separators, and frames use `#3E2A21` variants
- **Spore gold accents**: Checkmarks, slider grabs, and active states use `#FFE470`
- **Moss green headers**: Table headers and selection highlights use `#70E080`
- **High-contrast mode**: Uses brighter saturated fungal variants instead of blue tones
- **Consistent application**: All UI elements (buttons, tables, scrollbars, tabs) use the palette

## Acceptance Criteria
- [x] Panels no longer read as stock ImGui grey/blue
- [x] Button hover/active states use mushroom cap tones (warm brown to spore gold)
- [x] Table row backgrounds and headers match the fungal palette (moss green headers)
- [x] High-contrast preset remains visibly distinct (brighter saturated variants)
- [x] No regressions in readability on gameplay HUD or roster
- [x] All existing tests pass

## Testing
- All 19 unit test suites pass
- All 34 ImGui UI tests pass
- Client builds cleanly on Linux

Closes #324
