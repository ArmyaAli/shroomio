# AGENTS.md

## Repo Shape

- This is a C11 game repo driven by the root `Makefile`, not a JS workspace; ignore `.opencode/node_modules/` for product work.
- Main entrypoints: `src/client/main.c` for the raylib/ImGui client, `src/server/main.c` for the ENet/SQLite dedicated server, and `src/shared/sim.c` for deterministic gameplay rules.
- Keep `src/shared/` independent of raylib, ENet, and client/server code; client and server share simulation/protocol state through `src/shared/`.
- Core tuning constants live in `src/shared/config.h`; packet IDs, protocol version, ENet channels, and wire structs live in `src/shared/protocol.h`; world structs live in `src/shared/world.h`.
- Public project symbols use the `Shroom` prefix; gameplay state uses fixed-size arrays heavily, so check capacity constants before adding storage.

## Commands

- Build Linux client: `make client-linux` -> `dist/linux/client/shroomio`.
- Build Linux server: `make server-linux` or `make server` -> `dist/linux/server/shroomio-server`.
- Run local client: `make run`; run local server: `make run-server`.
- Cross-compile Windows: `make client-windows` and `make server-windows` require `mingw-w64`.
- Full local non-Valgrind gate: `make check` runs `make lint` then `make test`.
- CI also runs `make valgrind-test`, `make client-linux`, `make server-linux`, `make client-windows`, `make server-windows`, `make client-macos`, and `make server-macos`; `make check` alone is not the whole PR gate.
- Format C sources only: `make format`; check formatting: `make format-check` using `.clang-format`.
- Static analysis: `make cppcheck`; lint alias: `make lint`.
- Build spec PDF: `make spec` -> `dist/latex/shroomio-specification.pdf`.

## Dependencies And Tooling

- vcpkg is vendored in `vcpkg/`; Make installs dependencies under `vcpkg_installed/<platform>` with stamp files. Use `make vcpkg-install-linux`, `make vcpkg-install-windows`, or `make vcpkg-install` when dependency artifacts are missing.
- `make vendor` downloads Unity, test-only ImGui, and ImGui Test Engine into `vendor/`; tests may trigger these downloads through Make dependencies.
- Prefer the devcontainer when host tools are missing: `make devcontainer-build`, `make devcontainer-up`, `make devcontainer-exec CMD="make test"`, `make devcontainer-shell`.
- GitHub CLI is expected through the devcontainer in this repo: use `make devcontainer-gh ARGS="..."` and verify with `make devcontainer-github-status`.

## Testing Notes

- `make test` runs both Unity unit tests and the ImGui UI harness; use `make unit-test` or `make imgui-test` for narrower checks.
- A single Unity test binary can be built and run directly, e.g. `make build/tests/test_sim && ./build/tests/test_sim`.
- ImGui tests run under `xvfb-run` automatically when available; install/use the devcontainer if headless graphics dependencies are missing.
- ImGui tests isolate their working directory under `/tmp` and reset `client_settings.cfg`, `server_browser_recent.txt`, and `imgui.ini`; do not rely on repo-root copies for those tests.
- Memory gate: `make valgrind-test` runs unit tests plus a server smoke test. For UI leak investigations, use `make valgrind-imgui-test`; it reports third-party leaks without failing on them.
- Coverage: `make test-coverage` requires `gcovr` and writes HTML to `coverage/index.html`.

## Runtime And Ops

- The server is UDP/ENet on port `7777` by default; CLI flags override env vars, which override defaults: `--bind`/`SHROOM_SERVER_BIND`, `--port`/`SHROOM_SERVER_PORT`, `--database`/`SHROOM_SERVER_DB_PATH`.
- Server self-checks exist: `./dist/linux/server/shroomio-server --smoke-test --bind 127.0.0.1 --port 37777 --database /tmp/shroomio-smoke.db`.
- Deterministic performance baseline: `make benchmark` writes CSVs under `build/benchmarks/`; use the same command before/after optimization claims.
- Docker server image build copies ENet artifacts from `vcpkg_installed/linux/x64-linux`; run `make vcpkg-install-linux` before `make docker-server` if those files are absent.
- Docker Compose service maps `7777:7777/udp` and persists SQLite data in the `shroomio-server-data` volume.

## Workflow Constraints

- For issue/PR delivery, repo-local OpenCode guidance lives in `.opencode/agents/software-developer.md` and `.opencode/skills/github-backlog-delivery/SKILL.md`; it expects one PR per issue, feature branches from fresh `main`, devcontainer `gh`, CI watching, and no force-push.
- Database schema is pre-production and consolidated in `database/schema.sql`; update it directly rather than adding migrations unless the repo changes that policy.
- Asset exports belong under `assets/` with lowercase purpose-first names and `.source` before editable-source extensions; see `assets/README.md` and `docs/Cartoony-Art-Style.md` before adding art.
