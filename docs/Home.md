# shroomio

A real-time multiplayer fungal arena game written in C. Grow your fungal organism by collecting spores, consume smaller players, and climb a mass-based leaderboard — offline against bots or online against human opponents.

## Quick Links

- [Project Overview](Project-Overview) — what the game is, features, and architecture
- [Environment Setup](Environment-Setup) — how to set up a development environment
- [Development Guide](Development-Guide) — building, running, and contributing
- [Production Runbook](Production-Runbook) — deploying and operating the server in Docker
- [Refactor And Design Pattern Review](Refactor-Design-Review) — maintainability risks and follow-up refactor backlog

## Quick Start

```bash
# Build and run the client (offline, with bots)
make run

# Build and run the headless server
make run-server

# Run the server in Docker
make docker-run-server
```

## Repository

- **Source**: [github.com/ArmyaAli/shroomio](https://github.com/ArmyaAli/shroomio)
- **License**: MIT
- **Specification**: `design/shroomio-specification.tex` (compiled PDF at repo root)
