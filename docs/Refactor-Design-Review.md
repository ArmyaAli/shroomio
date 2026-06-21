# Refactor And Design Pattern Review

Issue: [#276](https://github.com/ArmyaAli/shroomio/issues/276)

Date: 2026-06-21

## Scope

This review covers the current maintainability shape of the gameplay client, screen layer, shared simulation, protocol definitions, server, and tests. It intentionally avoids behavior changes. The goal is to identify small, safe refactor slices that reduce regression risk before more gameplay, polish, audio, and UI work lands.

## Summary

The codebase is still small enough to refactor safely, and the strongest foundation is the shared simulation test coverage. The main risk is not algorithmic complexity; it is feature coupling in a few large modules:

- `src/client/game.c` is 3,265 lines and owns audio generation/playback, particles, combat feedback, overlays, prediction, snapshot application, camera, input-derived state, world drawing, HUD drawing, chat drawing, and session state.
- `src/server/main.c` is 1,571 lines and owns database schema/bootstrap, config parsing, ENet packet dispatch, lobby lifecycle, bot population, chat, auth, snapshots, and the server loop.
- `src/shared/sim.c` is 1,017 lines and is cohesive enough to remain a single simulation unit for now, but it mixes spawn rules, bot steering, spore grid collection, combat resolution, powerups, mass decay, split/merge, and stepping.
- `src/shared/protocol.h` centralizes the packet ABI and channel/reliability rules, which is useful, but it lacks a single declarative table tying packet type, channel, reliability, and size expectations together.
- Tests cover simulation and protocol behavior well, but client gameplay behavior is mostly validated indirectly through ImGui screen tests and production code paths.

The highest-value path is a sequence of extraction refactors with no gameplay changes: gameplay effects/audio first, then HUD/layout helpers, then server packet dispatch/schema ownership. Each should preserve public behavior and add focused tests where behavior is made testable.

## Top Architectural Risks

### 1. Gameplay Client Ownership Drift

`Game` has become a broad session container. Some fields are durable gameplay/session state, while others are render caches, transient UI state, particle baselines, audio state triggers, prediction buffers, chat state, and screen transition flags. This makes small polish changes risky because unrelated concerns share the same update/draw flow.

Concrete risk examples:

- `GameUpdate` computes input, split aim, network update, offline simulation, prediction, status banners, chat state, and render interpolation in one path.
- `DrawPlayers` renders entity bodies, local focus, split state, decay warnings, names, shield/speed effects, and split charge arrow visuals.
- Combat feedback is derived by comparing previous/current world snapshots in client state, so adding new gameplay events requires client-side inference instead of an explicit event contract.

Recommended direction:

- Keep `Game` as the owning aggregate, but move subsystems behind focused helper modules.
- Start with non-networked, low-risk helpers: particles/effects/audio and HUD/layout drawing.
- Avoid changing the simulation or network protocol as part of these extractions unless a dedicated issue requires it.

### 2. Implicit Gameplay Event Inference

Client-side feedback currently relies on local snapshot diffs and direct calls at action sites. This works, but future audio/effect polish will keep adding special-case inference.

Recommended pattern:

- Introduce a small client-only `GameplayEvent` queue first, populated from existing diff logic and local action triggers.
- Let particles, notifications, SFX, and screen flashes consume events from the queue.
- Keep the first version client-only and deterministic from existing state so it does not require protocol changes.
- Only add server-authored gameplay events later if a specific multiplayer desync or feedback gap requires it.

### 3. Screen Layout Consistency

The screen files are generally small and clear, but they repeat immediate-mode layout decisions directly in each screen. The gameplay screen also mixes input routing with state transitions.

Recommended pattern:

- Add small layout helpers for common panel sizing, centered modal windows, full-width action buttons, validation messages, and back navigation.
- Keep screen files declarative: screen-specific state plus calls to common layout helpers.
- Use ImGui tests to lock navigation and validation behavior before extracting helpers.

### 4. Simulation Rule Coupling

`sim.c` has good tests and clear static helpers, but new rules now share one update pipeline. Spawn protection, split/merge, mass decay, center-zone advantage, powerups, and bot targeting all interact through the same `ShroomPlayerState` fields.

Recommended pattern:

- Treat `ShroomWorldStep` as an ordered rule pipeline and document that order.
- Extract only pure rule predicates first, such as consume eligibility, decay eligibility, merge eligibility, split eligibility, and zone thresholds.
- Add table-driven unit tests around extracted predicates before changing rule internals.
- Avoid splitting `sim.c` into many files until the predicate seams are stable.

### 5. Protocol Evolution Safety

`protocol.h` is compact and easy to inspect, but packet rules are spread across enum values, packet structs, channel mapping, reliability mapping, size tests, and endpoint handlers. Version bumps are manual.

Recommended pattern:

- Add a packet metadata table or macro list that defines packet type, expected channel, reliability, and minimum size in one place.
- Generate or validate `ShroomPacketTypeToChannel` and `ShroomPacketTypeUsesReliableDelivery` from that metadata.
- Keep ABI size tests in `tests/unit/test_protocol.c` for every packet struct.
- Require a protocol version bump when packet wire layouts change.

### 6. Server Module Breadth

`server/main.c` is doing too much. It is still understandable, but database schema, lobby lifecycle, packet dispatch, chat rate limiting, auth handling, and tick loop changes all touch the same file.

Recommended pattern:

- First extract schema/bootstrap to `server/database.*` or `server/storage.*` with tests around initialization when practical.
- Then extract packet dispatch into a handler table or a small `server/packets.*` helper.
- Keep lobby state transitions near the server loop until packet dispatch and storage are separated.

## Recommended Design Patterns

### Gameplay Events

Use a small typed event queue owned by `Game`.

Event producers:

- Local input actions, such as split request accepted.
- Snapshot diff detection, such as player consumed, mass threshold crossed, or powerup appeared/collected.
- Session transitions, such as death cutscene start or respawn banner.

Event consumers:

- SFX playback.
- Particle bursts.
- Combat notifications.
- Screen flashes and banners.

Rules:

- Events are transient and frame-local unless explicitly queued with lifetime.
- Event payloads should contain stable IDs plus positions/masses needed by presentation.
- Simulation state remains authoritative; events should not mutate simulation rules.

### Audio And Effects

Use priority-gated presentation commands instead of calling SFX directly from many gameplay sites.

Rules:

- Gameplay code emits an event.
- The effect layer maps the event to sound, particles, notification, or flash.
- Importance/priority gating remains centralized so frequent events do not drown out critical feedback.
- Asset generation/loading remains separate from event consumption.

### HUD And Layout

Use small layout primitives rather than a full UI framework.

Recommended helpers:

- Centered fixed-size modal panel.
- Screen-edge anchored panel with margin and density scaling.
- Full-width action button.
- Standard validation/error row.
- Standard section heading and muted body text.

Rules:

- Screen-specific files still decide content and flow.
- Helpers only encode repeated sizing and visual conventions.
- Navigation behavior should remain covered by ImGui tests.

### Simulation Rules

Use named predicates and an explicit rule order.

Recommended order documentation:

1. Bot/player input is applied.
2. Movement and split impulses update positions.
3. Powerup timers and collection update state.
4. Spore collection and mass gain apply.
5. Consume eligibility resolves player-player interactions.
6. Mass decay and forced split rules apply.
7. Merge eligibility resolves split pieces.
8. Tick advances.

Rules:

- Predicates should be pure where possible.
- Mutating rules should remain in `sim.c` until tests clearly define safe seams.
- Any behavior-changing rule reorder needs a dedicated issue and tests.

### Protocol Rules

Use one metadata source for packet delivery behavior.

Recommended metadata fields:

- Packet type.
- Expected ENet channel.
- Reliability flag.
- Minimum packet size.
- Optional version note if layout changed recently.

Rules:

- Endpoints validate channel and minimum size before reading packet payloads.
- Packet layout changes bump `SHROOM_PROTOCOL_VERSION`.
- Tests assert packet sizes, channel mapping, reliability, and representative initialization.

## Prioritized Follow-Up Backlog

### P1: Extract Client Gameplay Event Queue ([#298](https://github.com/ArmyaAli/shroomio/issues/298))

Goal: Centralize presentation triggers for SFX, particles, combat notifications, and screen flashes without changing gameplay behavior.

Suggested scope:

- Add a small client-only event type and fixed-capacity queue.
- Route existing local split, death, mass-gain, and consume feedback through the queue.
- Keep existing visuals and sounds unchanged.
- Add unit-testable helper coverage where possible and run ImGui smoke tests.

Acceptance checks:

- Existing gameplay feedback still appears for split, consume, death, and respawn.
- No protocol changes.
- `make test`, `make format-check`, and `make cppcheck` pass.

### P2: Extract Gameplay Audio And Effect Dispatch ([#299](https://github.com/ArmyaAli/shroomio/issues/299))

Goal: Separate generated sound loading/playback and event-to-effect mapping from `game.c` drawing and update logic.

Suggested scope:

- Move client SFX/music loading and priority playback to a focused helper module.
- Add a thin effect dispatcher that consumes gameplay events.
- Keep public entry points small, for example `ClientEffectsUpdate`, `ClientEffectsEmit`, and `ClientEffectsShutdown`.

Acceptance checks:

- No changes to sound timing or volume behavior except fixes explicitly covered by tests/manual notes.
- `game.c` no longer owns generated SFX implementation details.

### P3: Add Gameplay HUD/Layout Helpers ([#301](https://github.com/ArmyaAli/shroomio/issues/301))

Goal: Standardize repeated gameplay panel and modal layout without changing screen content.

Suggested scope:

- Extract centered modal, anchored panel, full-width button, and validation row helpers.
- Apply first to main menu, settings, server browser, lobby browser, and gameplay overlays in small batches.
- Extend ImGui tests around any moved controls.

Acceptance checks:

- Existing ImGui navigation tests pass.
- No visible regression in button labels, validation messages, or screen transitions.

### P4: Extract Simulation Rule Predicates ([#300](https://github.com/ArmyaAli/shroomio/issues/300))

Goal: Make simulation interactions safer to modify by naming eligibility checks and adding focused tests.

Suggested scope:

- Extract pure predicates for consume, decay, split, merge, and spawn protection eligibility.
- Keep mutation in `sim.c`.
- Add table-style unit tests around boundary values.

Acceptance checks:

- No rule order changes.
- Existing simulation tests pass unchanged.
- New predicate tests cover center/outer zone thresholds, shield protection, split spawn protection, and merge cooldown boundaries.

### P5: Consolidate Protocol Packet Metadata ([#303](https://github.com/ArmyaAli/shroomio/issues/303))

Goal: Remove drift between packet enum, channel mapping, reliability mapping, and protocol tests.

Suggested scope:

- Define packet metadata in one internal macro list or table.
- Drive `ShroomPacketTypeToChannel` and `ShroomPacketTypeUsesReliableDelivery` from that source.
- Add tests that every known packet type has expected channel and reliability.

Acceptance checks:

- Packet struct layouts do not change.
- Protocol version does not change unless a wire layout changes.
- Protocol unit tests remain explicit about ABI sizes.

### P6: Split Server Database Bootstrap From Server Loop ([#302](https://github.com/ArmyaAli/shroomio/issues/302))

Goal: Move schema creation, seed-data execution, and database path handling out of `server/main.c`.

Suggested scope:

- Add `src/server/database.c` and `src/server/database.h` for schema initialization and SQL file execution.
- Keep auth queries in `auth.c` for now.
- Add a small test or smoke target if practical; otherwise rely on server startup and Valgrind smoke.

Acceptance checks:

- Server starts with an empty database and creates schema as before.
- Seed mushroom species still load.
- `make server-linux`, `make test`, and `make valgrind-test` pass.

### P7: Add Server Packet Dispatch Table ([#304](https://github.com/ArmyaAli/shroomio/issues/304))

Goal: Reduce packet handling branch growth in `server/main.c`.

Suggested scope:

- Introduce a small dispatch table from packet type to handler.
- Keep handler implementations in `server/main.c` initially.
- Validate packet channel and minimum size before dispatch.

Acceptance checks:

- Existing auth, lobby, chat, ping, and input flows behave the same.
- Protocol/channel tests cover invalid channel handling where practical.

## Work Not Recommended Yet

- Do not split `Game` into many owning structs in one pass. That would touch too much behavior without clear safety gains.
- Do not introduce dynamic allocation for gameplay queues or simulation state. Fixed capacities match the project conventions.
- Do not add server-authored gameplay event packets until client-only event routing proves insufficient.
- Do not reorganize packet structs in `protocol.h` unless a protocol version bump and compatibility decision are explicitly scoped.

## Suggested Merge Gates For Refactor Issues

Use these as default gates unless an issue explains why a narrower set is sufficient:

- `make test`
- `make format-check`
- `make cppcheck`
- `make server-linux` for server/shared changes
- `make client-linux` for client/shared changes
- `make valgrind-test` for server lifecycle, allocation, or packet handling changes

## Immediate Outcome

No behavior-changing refactor is included in this review branch. The next safe step is to create dedicated follow-up issues from the backlog above and deliver them one at a time.
