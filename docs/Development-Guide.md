# Development Guide

## Building

All build targets use the root `Makefile`. The Makefile downloads raylib source on first build and compiles it statically.

## Devcontainer Tooling

The project devcontainer includes:

- `gh` via the GitHub CLI devcontainer feature
- Docker client access from the non-root `dev` user via Docker-outside-of-Docker

After rebuilding the container you should be able to run:

```bash
gh auth status
docker version
```

If Docker access fails after a rebuild, reopen the workspace in the container so the updated group mapping is applied.

### Linux Client

```bash
make client-linux # Builds dist/linux/client/shroomio
make run         # Builds and launches the client
make imgui-test  # Builds and runs the ImGui UI test harness
make gameplay-visual-test  # Runs exploratory gameplay/HUD/overlay visual checks
```

The exploratory target stresses dense core-gameplay presentation at the supported UI-scale and HUD-density
endpoints, including long labels, collectibles, markers, overlays, and Lobby Browser entries. It uses the
same headless ImGui harness as `make imgui-test`; failures identify the affected screen/component.

#### WSLg audio recovery soak

Capture client output with `make run 2>&1 | tee /tmp/shroomio-wslg-audio.log`. For five minutes, repeatedly
suspend and restore the WSLg PulseAudio endpoint while moving between the main menu and active matches. During
each interruption, Settings should report that audio was lost and is retrying; music and effects should resume
within eight seconds after the endpoint returns, without selecting **Restart Audio**. Save the terminal log and
record any cycle that does not return to `Audio ready`. Use `make run-windows` for normal WSL development when
testing the Linux audio backend itself is not required.

### Linux Headless Server

```bash
make server-linux # Builds dist/linux/server/shroomio-server
make run-server  # Builds and launches the server
```

### Windows Client (cross-compile from Linux)

```bash
make client-windows # Builds dist/windows/client/shroomio.exe
```

Requires `mingw-w64` installed. Produces a fully static `.exe`.

### macOS (native Intel build)

macOS builds are native rather than cross-compiled. The supported build and release lane currently uses an
Intel host and the vcpkg `x64-osx` triplet; Apple Silicon native artifacts are not produced or validated yet.

Install Xcode Command Line Tools and the same build helpers used by CI:

```bash
xcode-select --install
brew install cmake ninja pkg-config
```

From the repository root, bootstrap dependencies and build both programs:

```bash
make vcpkg-install-macos
make client-macos       # dist/macos/client/shroomio
make server-macos       # dist/macos/server/shroomio-server
```

Validate the headless server without leaving a persistent database:

```bash
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
./dist/macos/server/shroomio-server --smoke-test \
  --bind 127.0.0.1 --port 37777 --database "$tmpdir/smoke.db"
```

Launch `./dist/macos/client/shroomio` from a logged-in desktop session for manual graphics, input, and audio
testing. CI verifies that the client is an x86-64 Mach-O executable and records its linked dependencies, but does
not claim GUI runtime coverage because hosted runners do not provide the required interactive session.

#### Signing and notarization

Local builds and the current GitHub release archives are development artifacts: the workflow does not apply a
Developer ID signature or submit them to Apple's notary service. Do not describe those archives as notarized.
Developer builds may be run directly on the build Mac; public distribution should instead:

1. Package the client as a macOS `.app` bundle and sign all bundled code with a **Developer ID Application**
   certificate, hardened runtime, and a trusted timestamp.
2. Verify signatures with `codesign --verify --deep --strict --verbose=2 <bundle>` and assess the bundle with
   `spctl --assess --type execute --verbose=2 <bundle>`.
3. Submit a signed ZIP, DMG, or PKG with `xcrun notarytool submit ... --wait`, then staple and validate the
   ticket on a supported bundle/container using `xcrun stapler`.
4. Keep certificates, App Store Connect API keys, and keychain passwords in a protected CI environment. Never
   commit signing credentials or expose them to pull-request jobs.

Release signing/notarization should be added only when those protected credentials and an app-bundle packaging
step are available. The headless server must remain smoke-tested after any signing or packaging change.

## Running

### Local Play (offline)

```bash
make run
```

Launches the client with 18 bots and full simulation running locally. No network required.

### Server Play (online)

**Terminal 1 — start the server:**

```bash
make run-server
```

The server listens on UDP port 7777 and spawns 18 bots.

**Terminal 2 — start a client:**

```bash
make run
```

The client attempts to connect to `127.0.0.1:7777`. After successful handshake, the server controls the simulation authoritatively.

## Profiling and benchmarking

- `docs/Profiling-Instrumentation.md` documents opt-in runtime timing logs.
- `docs/Benchmarking.md` documents `make benchmark` and CSV benchmark artifacts.
- `docs/Profiling-Runbook.md` documents CPU, memory, and network profiling workflows.

### Docker Deployment

```bash
make docker-server        # Build the server image
make docker-run-server    # Build and run (map UDP 7777)
```

Or with Docker Compose:

```bash
docker compose up --build server
```

## Devcontainer

Use the devcontainer targets when you need tooling that only exists in the container, such as `gh`.

```bash
make devcontainer-build
make devcontainer-up
make devcontainer-exec CMD="make test"
make devcontainer-gh ARGS="issue list --limit 20"
```

`make devcontainer-shell` opens an interactive shell if you want to work inside the container directly.

## Project Structure

```
.
├── .devcontainer/          # VS Code devcontainer config
├── .github/
│   └── ISSUE_TEMPLATE/     # Bug report and feature request templates
├── assets/                 # Placeholder dir for sprites, audio, maps, shaders
├── build/                  # Intermediate build artifacts (object files)
├── design/
│   └── shroomio-specification.tex  # LaTeX software specification
├── dist/                   # Output binaries grouped as <platform>/client/ and <platform>/server/
├── src/
│   ├── client/
│   │   ├── main.c          # Entry point, window creation, render loop
│   │   ├── game.h          # Game struct and lifecycle declarations
│   │   ├── game.c          # Init, update, draw, camera, UI, input
│   │   ├── net.h           # Client net state and API
│   │   └── net.c           # ENet host, handshake, input send, snapshot recv
│   ├── server/
│   │   └── main.c          # ENet host, session mgmt, tick loop, snapshots
│   └── shared/
│       ├── config.h        # All compile-time constants
│       ├── world.h         # WorldState, PlayerState, SporeState, Zone
│       ├── sim.h           # Simulation API
│       ├── sim.c           # Movement, spores, combat, bot AI, spawning
│       ├── protocol.h      # Binary packet format definitions
│       └── vec2.h          # 2D vector inline math
├── vendor/
│   └── enet/               # Vendored ENet source (for server builds)
├── Dockerfile.server       # Server Docker image
├── compose.yaml            # Docker Compose for server
├── Makefile                # All build targets
├── LICENSE                 # MIT License
└── README.md               # Project README
```

## Code Conventions

- Language: C11 (`-std=c11`)
- Compiler warnings: `-Wall -Wextra -Wpedantic`
- Naming: `Shroom` prefix on all public types and functions (e.g., `ShroomWorldStep`)
- All types and functions exposed in headers; statics used for internal helpers
- No dynamic allocation beyond ENet internals; fixed-size arrays used throughout
- Shared code must not include raylib or enet headers

## Key Files to Know

| File | Purpose |
|------|---------|
| `src/shared/config.h` | Change tuning parameters here (speeds, spawns, spore counts, ratios) |
| `src/shared/protocol.h` | Define or extend packet types here |
| `src/shared/world.h` | Define or extend world/player/spore state here |
| `src/shared/sim.c` | Add or modify simulation rules here |
| `src/client/game.c` | Modify rendering, HUD, or client-side game logic here |
| `src/client/net.c` | Modify client-side networking here |
| `src/server/main.c` | Modify server networking or session logic here |

## Building the Specification PDF

```bash
latexmk -pdf -output-directory=. design/shroomio-specification.tex
```

The PDF is written to `shroomio-specification.pdf` at the repo root.

## Testing

Automated testing workflow:

1. `make test` runs the C unit suite plus the ImGui screen tests.
2. `make imgui-test` runs only the ImGui screen harness.
3. `make e2e` runs the UI harness plus live client/server integration probes for discovery, health, REST, authentication, shutdown persistence, chat logging, snapshot cadence, and input admission. The probes isolate databases and UDP ports under `/tmp`.
4. `make valgrind-test` runs unit tests plus a server startup/shutdown smoke test under Valgrind and fails on memory errors or definite leaks.
5. `make valgrind-imgui-test` runs a focused ImGui harness under Valgrind for UI investigations; third-party library leaks are reported but do not fail this target.

From the host, use the devcontainer wrapper when Valgrind is not installed locally:

```bash
make devcontainer-exec CMD="make valgrind-test"
```

Manual testing workflow:

1. **Offline**: `make run` — verify rendering, input, spore collection, consumption, respawn, leaderboard.
2. **Server**: `make run-server` then `make run` — verify handshake, snapshots, bot behavior.
3. **Docker**: `make docker-run-server` then connect a client.
4. **Windows**: `make client-windows` — copy `dist/windows/client/shroomio.exe` to a Windows machine and run.

## Protocol Notes

`src/shared/protocol.h` defines both packet layout and channel assignment. When adding new packet types:

1. Assign them to the correct ENet channel with `ShroomPacketTypeToChannel()`
2. Mark whether they require reliable delivery with `ShroomPacketTypeUsesReliableDelivery()`
3. Add or update protocol unit tests in `tests/unit/test_protocol.c`

Use reliable channels for control flow and chat, and unreliable channels for fast-expiring state such as snapshots, input, and voice frames.

## Debugging Tips

- The server prints peer connect/disconnect events to stdout.
- The client displays connection status in the HUD (top-left, green/orange).
- Snapshot tick is visible in the HUD to confirm data flow.
- Use `gdb` for native debugging: `gdb ./dist/linux/client/shroomio`.
- The client can run offline even if the server is unreachable — it falls back to local simulation.
