# shroomio

Minimal `raylib` game scaffold in pure C with a single `Makefile`.

Project layout:

- `src/client/`: raylib client entry point and rendering
- `src/shared/`: shared simulation state and rules
- `src/server/`: headless server entry point scaffold
- `assets/`: place game data here
- `dist/`: built binaries

## Requirements

Linux build:

- `make`
- `curl`
- a C compiler such as `gcc` or `clang`
- Linux desktop development libraries for OpenGL/X11

Windows build from WSL:

- `mingw-w64`

Example install on Ubuntu/WSL:

```bash
sudo apt update
sudo apt install build-essential curl mingw-w64 libx11-dev libxcursor-dev libxrandr-dev libxi-dev libgl1-mesa-dev libasound2-dev
```

## Build

```bash
make linux
make server
make windows
```

Convenience targets:

```bash
make run
make run-server
make docker-server
make docker-run-server
```

Artifacts are written to `dist/`:

- `dist/shroomio`
- `dist/shroomio-server`
- `dist/shroomio.exe`

## Current scaffold

- Local sandbox arena with bot colonies
- Mouse-directed movement with temporary WASD fallback
- Spore collection, growth, size-based speed scaling, and consume/respawn loop
- Center hotspot, mid ring, and outer ring arena zones
- Leaderboard and shared world/player simulation state
- ENet-backed headless server listening on UDP `7777`
- Client/server handshake, input sending, and authoritative player snapshot prototype
- Shared world/player simulation state separated from client rendering
- `Camera2D` centered on the player
- World grid for motion/readability
- Asset folders ready for sprites, audio, maps, and shaders

## Server Container

Build and run the headless server in Docker:

```bash
make docker-server
make docker-run-server
```

The container exposes UDP port `7777`.

## Docker Compose

Run the server with Docker Compose:

```bash
docker compose up --build server
```

## Dev Container

The repo includes a VS Code dev container under `.devcontainer/` with:

- `C11` toolchain via `build-essential`
- Linux desktop build dependencies for `raylib`
- `mingw-w64` for `make windows`
- Docker CLI support inside the dev container
- GitHub CLI via `gh`
- latest `opencode` via `npm install -g opencode-ai`

Open the repo in VS Code and choose `Reopen in Container`.

Without VS Code, use the Dev Container CLI:

```bash
make devcontainer-build
make devcontainer-up
make devcontainer-shell
make devcontainer-down
```

These Makefile targets use the `.devcontainer/Dockerfile` directly with plain Docker so they work even when the Dev Container CLI has issues under WSL/buildx.

The devcontainer mounts your WSL `~/.ssh` into `/home/dev/.ssh` as read-only.
It also mounts your host `~/.gitconfig` into `/home/dev/.gitconfig` as read-only.
For GitHub CLI auth, use a host-side token file stored outside the repo.
It also mounts your WSL OpenCode directories read-only and copies them into the container on startup so the container reuses your existing setup without sharing the live SQLite/state files:

- `~/.config/opencode` -> `/home/dev/.config/opencode`
- `~/.local/share/opencode` -> `/home/dev/.local/share/opencode`
- `~/.local/state/opencode` -> `/home/dev/.local/state/opencode`
- `~/.cache/opencode` -> `/home/dev/.cache/opencode`

To set a persistent GitHub token for the devcontainer:

```bash
make devcontainer-github-token
make devcontainer-down
make devcontainer-up
make devcontainer-github-status
```

The token is stored on the host at:

- `~/.config/shroomio-devcontainer/github-token`

with restrictive permissions and mounted into the container as a read-only secret file.

## Notes

- The `Makefile` downloads `raylib` source on demand and builds a static `libraylib.a` for each target.
- The Windows build uses `-static` to produce a self-contained `.exe`.
- Fully static Linux binaries are usually not practical with desktop/X11 system libraries, so the Linux target links `raylib` statically while still using system libraries dynamically.
# shroomio
# shroomio
# shroomio
# shroomio
