# Project Overview

## What is shroomio?

shroomio is a real-time multiplayer arena game inspired by mass-growth and territory-pressure games. Players control fungal organisms in a large shared map, collect spores to grow, avoid larger threats, and consume smaller organisms to climb a mass-based leaderboard.

## Gameplay

### Core Loop

1. Enter the arena at low-to-mid mass
2. Collect spores in the outer ring to grow safely
3. Rotate toward mid and center zones to improve growth rate
4. Watch for favorable fights against smaller targets
5. Avoid larger predators — increased mass lowers mobility
6. Climb the leaderboard
7. Respawn quickly and re-engage after defeat

### Key Mechanics

- **Spore collection**: The reliable growth path. 900 spores populate the arena, biased toward center and mid zones to create pressure toward contested space.
- **Consumption**: A player can consume a smaller player if they have at least a 1.15x mass advantage. The attacker gains 70% of the victim's mass.
- **Speed scaling**: Higher mass = slower movement, creating meaningful vulnerability for large players.
- **Fast respawn**: Defeat triggers immediate respawn at default mass in a safe outer-zone location.

### Arena

| Zone  | Radius  | Description |
|-------|---------|-------------|
| Center | 0–900 | Highest traffic and conflict pressure |
| Mid   | 900–2000 | Transition space with moderate contest |
| Outer | 2000–3000 | Safer spawn and recovery space |

## Features

- **Offline mode**: Play against 18 bots with full simulation, spore economy, and leaderboard.
- **Online multiplayer**: Connect to a headless dedicated server (UDP port 7777) with authoritative simulation.
- **Dual-mode client**: Seamlessly switches between offline local simulation and server-authoritative snapshots.
- **Bot AI**: Utility-driven behavior — flee threats, chase prey, forage spores, drift toward center.
- **Cross-platform builds**: Linux client, Linux headless server, and Windows client cross-build from Linux.

## Architecture

```
src/
  client/          # raylib rendering, input, camera, UI, ENet client
    main.c           entry point, window creation
    game.c / game.h  game state, update loop, 2D rendering, camera
    net.c  / net.h   ENet host lifecycle, handshake, input/snapshot I/O
  server/          # headless dedicated server
    main.c           ENet host, session management, tick loop
  shared/          # shared between client and server
    config.h         compile-time tuning constants
    world.h          data types (WorldState, PlayerState, SporeState, Zone)
    sim.c / sim.h    deterministic simulation, movement, spores, combat, bot AI
    protocol.h       binary packet definitions for all client-server messages
    vec2.h           inline 2D vector math
```

### Key Design Rules

- Shared code (`src/shared/`) has no dependency on raylib, ENet, client, or server code.
- Client code may depend on shared but never on server.
- Server code may depend on shared but never on client.
- Simulation rules are centralized in `src/shared/sim.c` and `src/shared/config.h`.

### Network Channels

The protocol uses dedicated ENet channels so latency-sensitive traffic does not block reliable control flow:

| Channel | Reliability | Purpose | Current / Planned Packets |
|---------|-------------|---------|----------------------------|
| 0 `CONTROL` | Reliable, ordered | handshake, auth, lifecycle | `HELLO`, `WELCOME`, `PING`, `PONG`, `AUTH_REQUEST`, `AUTH_RESPONSE` |
| 1 `SNAPSHOT` | Unreliable, fire-and-forget | world replication | `SNAPSHOT`, `SPORE_STATE` |
| 2 `INPUT` | Unreliable, ordered by sequence | player movement input | `INPUT` |
| 3 `CHAT` | Reliable, ordered | text communication | `CHAT` |
| 4 `VOICE` | Unreliable, ordered | real-time voice payloads | `VOICE_FRAME` |

This layout keeps stale snapshots and voice packets from clogging reliable control and chat traffic.

## Tech Stack

| Component | Technology |
|-----------|------------|
| Language | C11 |
| Client graphics | raylib 5.5 (static linking) |
| Networking | ENet (UDP, port 7777) |
| Build system | GNU Make |
| Containerization | Docker, Docker Compose |
| Dev environment | VS Code devcontainer, Debian Bookworm |
| Windows cross-build | mingw-w64 |
| Spec document | LaTeX (pdflatex, latexmk) |

## License

MIT License — see [LICENSE](https://github.com/ArmyaAli/shroomio/blob/main/LICENSE)
