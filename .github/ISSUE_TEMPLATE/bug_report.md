---
name: Bug Report
about: Report a bug in shroomio (client, server, or shared simulation)
title: "[Bug] "
labels: ["bug"]
assignees: []
---

## Summary

A clear and concise description of the bug.

## Reproduction Steps

1. Build target: `make client-linux` / `make server-linux` / `make client-windows`
2. Run mode: offline / connected to server at `...`
3. Steps to trigger:
   - ...
   - ...

## Expected Behavior

What you expected to happen.

## Observed Behavior

What actually happened. Include crash output, segfaults, or log lines if applicable.

## Environment

| Field | Value |
|-------|-------|
| OS | e.g. Ubuntu 24.04, Windows 11 |
| Build target | `make client-linux`, `make server-linux`, `make client-windows` |
| Network mode | offline / online (server address) |
| Commit | branch @ `abc1234` |
| Compiler | gcc/clang version |

## Affected Module

- [ ] Client (`src/client/`)
- [ ] Server (`src/server/`)
- [ ] Shared simulation (`src/shared/sim.c`)
- [ ] Shared protocol (`src/shared/protocol.h`)
- [ ] Build system (`Makefile`)

## Severity

- [ ] Crash / segfault
- [ ] Gameplay logic incorrect
- [ ] Visual / rendering glitch
- [ ] Networking desync or disconnect
- [ ] Build failure
- [ ] Other

## Additional Context

Screenshots, packet captures, valgrind output, or anything else relevant.
