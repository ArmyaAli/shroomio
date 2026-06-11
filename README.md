# shroomio

A real-time multiplayer arena game built in pure C. Control a fungal organism, collect spores to grow, consume smaller players, and climb the leaderboard in a mass-growth battle inspired by agar.io.

[![CI](https://github.com/ArmyaAli/shroomio/actions/workflows/ci.yml/badge.svg)](https://github.com/ArmyaAli/shroomio/actions/workflows/ci.yml)
[![CodeQL](https://github.com/ArmyaAli/shroomio/actions/workflows/codeql.yml/badge.svg)](https://github.com/ArmyaAli/shroomio/actions/workflows/codeql.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## Features

- **Offline mode** — play against 18 bot colonies with full simulation, spore economy, and leaderboard
- **Online multiplayer** — authoritative headless server over ENet/UDP (port 7777)
- **Dual-mode client** — seamless switching between local sandbox and server-authoritative play
- **Bot AI** — utility-driven behavior: flee threats, chase prey, forage spores, contest the center
- **Arena zones** — center hotspot, mid ring, and outer ring with escalating risk and reward
- **Cross-platform** — Linux client, Windows client (cross-compiled from WSL), Linux dedicated server

## Tech Stack

| Component | Technology |
|---|---|
| Language | C11 |
| Graphics | raylib 5.5 (static) |
| Networking | ENet (UDP) |
| Build | GNU Make |
| Containers | Docker, Docker Compose |
| Dev Environment | VS Code Dev Container (Debian Bookworm) |
| Cross-compile | mingw-w64 |

## Project Structure

```
src/
  client/        raylib rendering, input, camera, UI, ENet client
  server/        headless dedicated server, session management, tick loop
  shared/        deterministic simulation, protocol, world state, vector math
tests/
  unit/          unit tests (Unity framework)
  unity/         Unity test framework source
assets/          sprites, audio, maps, shaders
dist/            built binaries
design/          LaTeX game specification
docs/            development guides and runbooks
```

The `shared/` module has zero dependencies on raylib, ENet, client, or server code. Client and server never depend on each other.

## Prerequisites

**Linux / WSL:**

```bash
sudo apt update
sudo apt install build-essential curl \
  libx11-dev libxcursor-dev libxrandr-dev libxi-dev libxinerama-dev \
  libgl1-mesa-dev libasound2-dev libsqlite3-dev
```

**Windows cross-compile from WSL** (additional):

```bash
sudo apt install mingw-w64
```

## Building

```bash
make linux      # Linux client    -> dist/shroomio
make server     # Linux server    -> dist/shroomio-server
make windows    # Windows client  -> dist/shroomio.exe  (requires mingw-w64)
```

Run directly:

```bash
make run            # build and run the Linux client
make run-server     # build and run the server
```

The Makefile downloads raylib 5.5 source on demand and builds a static `libraylib.a` per target. The Windows build uses `-static` for a self-contained `.exe`.

## Testing

Run the unit test suite:

```bash
make test
```

The project uses [Unity](https://github.com/ThrowTheSwitch/Unity) for unit testing. Tests are located in `tests/unit/` and cover core modules like vector math and protocol structures.

To run tests with coverage reporting (requires `gcovr`):

```bash
make test-coverage    # generates coverage.html
```

To add new tests:
1. Create a test file in `tests/unit/` (e.g., `test_mymodule.c`)
2. Include Unity and the module under test
3. Write test functions using `TEST_ASSERT_*` macros
4. Add tests to the `main()` function with `RUN_TEST()`

## Running the Server

**With Docker Compose** (recommended):

```bash
docker compose up --build server
```

**With Make targets:**

```bash
make docker-server        # build the server image
make docker-run-server    # build and run (exposes UDP 7777)
make docker-logs          # follow container logs
```

**Native:**

```bash
make run-server
```

The server listens on UDP port `7777`.

## Dev Container

The repo includes a VS Code dev container (`.devcontainer/`) with the full C11 toolchain, raylib build dependencies, mingw-w64, Docker CLI, and GitHub CLI preconfigured.

**VS Code:** Open the repo and select **Reopen in Container**.

**CLI (without VS Code):**

```bash
make devcontainer-build
make devcontainer-up
make devcontainer-shell
make devcontainer-down
```

The dev container mounts your host `~/.ssh`, `~/.gitconfig`, and OpenCode directories read-only so credentials and tooling carry through. To configure GitHub CLI authentication:

```bash
make devcontainer-github-token    # store a token on the host
make devcontainer-down && make devcontainer-up
make devcontainer-github-status   # verify auth
```

## Game Specification

A full LaTeX specification covering game mechanics, tuning constants, and protocol design is available at `design/shroomio-specification.tex`. Build the PDF with:

```bash
make spec    # -> dist/latex/shroomio-specification.pdf
```

## CI

Pull requests are validated on GitHub Actions with:

- **clang-format** — enforces `.clang-format` style across `src/`
- **cppcheck** — static analysis for warnings, style, performance, and portability
- **Unit tests** — runs the test suite via `make test`
- **Build** — compiles both the Linux client and headless server

## License

MIT — see [LICENSE](LICENSE).
