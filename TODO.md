# Shroomio Handoff TODO

Last updated: 2026-06-13

## Current State

- `main` is up to date through vcpkg migration (#172).
- All original MVP issues are closed: message channels (#27), text chat (#31), lobby browser (#34), latency monitoring (#29), client-side prediction (#8).
- Recent merges: zone mechanics (#167), large-colony mass rules (#166), dynamic bot scaling (#168), vcpkg dependency migration (#172), scroll-wheel zoom (#159), arrow indicators (#163), chat timestamps (#162).
- Stale open issues #166, #167, #168, #172 closed to match merged state.

## Completed Recently

- Merged zone-based arena mechanics (#167).
- Merged large-colony mass cap / speed floor / decay rules (#166).
- Merged dynamic bot scaling for lobbies (#168).
- Merged vcpkg migration for raylib, imgui, enet (#172).
- Merged scroll-wheel zoom (#159), off-screen arrow indicators (#163), chat timestamps (#162).
- Merged ImGui test harness port to plain C11 (#160) via imgui_te_wrapper adapter.
- Closed stale open issues that had already been merged.

## Next Issues (Priority Order)

### High Priority
1. #165 Player splitting mechanic when colony grows too large.

### Medium Priority
2. #164 Powerup pickups distributed across the arena.
3. #91  Keybinding UI for gameplay, chat, and communication hotkeys.
4. #88  Pre-game lobby roster panel with readiness and countdown placeholders.
5. #85  Post-match results screen for replay and exit flow.
6. #83  Connection-state modal (connect, handshake, failure, retry).
7. #79  Nearby threat and prey outline states based on consume rules.

### HUD / UX Pass
8. #97  Reduce HUD clutter, move leaderboard/settings into overlay modals.
9. #80  Compact, full, and minimal HUD modes with persisted preference.
10. #81 Escape-menu for offline pause and online leave-match confirmation.
11. #76 Rework gameplay HUD into player-readable v1 layout.
12. #77 Zone transition callouts and persistent zone risk/reward legend.
13. #78 Off-screen threat and prey edge indicators.
14. #75 Death and respawn overlay with countdown and re-entry messaging.

### Help Screen
15. #107 Redesign help screen with polished visual layout.
16. #108 Tabbed or section-based navigation.
17. #109 Animated zone visualization.
18. #110 Interactive growth and consume demo.
19. #111 Animated control scheme preview.

### Profiling / Benchmarking
20. #72 Instrument client and server frame/tick timings.
21. #73 Build repeatable server and multiplayer benchmark harness.
22. #74 Add profiling runbook for CPU, memory, and network hot paths.

### Website (not started)
23. #120 Set up Astro project structure in website/ folder.
24. #121 Create landing page layout and hero section.
25. #122 Create download page with platform-specific binary links.
26. #123 Add game features and screenshots section.
27. #124 Add styling and responsive design with CSS/Tailwind.
28. #125 Configure build pipeline and deployment.
29. #127 Add WebAssembly (WASM) compile target for browser play.
30. #128 Add WASM build to website download page.

### Polish / Content (M5)
31. #136 Define and implement fungal visual identity for menus and HUD.
32. #135 Add fungal arena art pass for spores, colonies, and zone presentation.
33. #134 Add mushroom-themed audio and feedback cues.

### Post-MVP
- #28  Implement basic client-side prediction (tracking issue).
- #62  End-to-end testing framework.
- #60  Fullscreen mode with quick toggle.
- #59  Responsive window resizing with camera anchoring.
- #57  Auto-updater for client.
- #56  Localization and multi-language support.
- #55  Replay system.
- #54  Teams mode and battle royale.
- #126 macOS compile target.
- #157 Transport-layer encryption for ENet protocol.
- #160 Port ImGui test harness to C bindings. **Done** (#179).

## Process Notes

- Use GitHub issues for all work.
- Sync `main` before starting new work.
- Create feature/fix branches for changes.
- Open PRs for changes.
- Use rebase merges going forward.
- Code must compile before committing.
- Run the relevant local build/test targets before pushing.
- Run `make format-check` before pushing any C files.
- Keep pre-production database schema consolidated in `database/schema.sql` rather than incremental migrations.
- Close GitHub issues after merging the implementing PR.

## Useful Verification Commands

- `make test`
- `make linux`
- `make server`
- `make format-check`
- `make cppcheck` if `cppcheck` is installed
- `timeout 2 ./dist/shroomio-server`

## Known Local Artifacts

- Server smoke tests generate `shroomio.db` and sometimes `shroomio.db-journal` in the repo root.
- These files should not be committed.
