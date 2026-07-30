# Atlas

Atlas is a compile-time composed, server-authoritative C++23 platform for building real-time interactive
applications — dedicated servers, gameplay clients, editors, automated test harnesses, CLI tools. It provides a
deterministic runtime, compile-time capability composition, generated contracts, reflection, serialization,
scheduling, replication, and resource identity.

## Why

Three principles govern everything in Atlas:

- **Atlas defines execution. Capabilities define behavior. Applications define meaning.** The runtime
  understands entities, properties, requests, events, stages, jobs, resources, scheduling — it never
  understands players, health, weapons, inventories, quests, or any other application semantics.
- **All composition happens at compile time.** There is no plugin discovery and no runtime capability
  registration. An invalid dependency graph, contract, or stage ordering is a compile-time build failure, not a
  runtime surprise.
- **Bit-exact determinism is a hard guarantee, not best-effort.** Identical inputs produce identical outputs
  down to the bit, across machines and across full-session replay.

## What you can build with it

Every host — server, client, editor, test harness, CLI tool — is composed from the same capability libraries
under the same architectural model, so a headless dedicated server and a full editor share one substrate rather
than diverging codebases. `demo/` is a growing gameplay demo built on top of Atlas, proving out request
dispatch, property composition, and replication against real (if minimal) gameplay capabilities as they're
built — see `demo/README.md`.

**This repository is early-stage.** Eleven real libraries are seeded (`atlas-core`, `atlas-entity`,
`atlas-contracts`, `atlas-stage`, `atlas-resource`, `atlas-serialization`, `atlas-reflection`, `atlas-request`,
`atlas-scheduler`, `atlas-replication`, `atlas-runtime`), plus a first tool, `tools/atlas-cgen` (a manifest-to-
C++ contract generator). Five optional libraries (`atlas-input`, `atlas-ui`, `atlas-render`, `atlas-audio`, `atlas-editor`) are still
status-stubs.

## Building

```sh
# One-time, per clone: wire up the mandatory hooks (gitleaks + format/lint gates)
git config core.hooksPath .githooks

cmake --preset debug && cmake --build --preset debug && ctest --preset debug     # sanitized debug, day-to-day loop
cmake --preset release && cmake --build --preset release && ctest --preset release

clang-format -i $(git ls-files '*.cpp' '*.hpp')   # format
cmake --preset clang-tidy && cmake --build --preset clang-tidy    # static analysis
cmake --preset coverage && cmake --build --preset coverage && ctest --preset coverage && \
  cmake --build build/coverage --target coverage  # coverage gate (75% line + branch)
```

Requires a C++23 toolchain (GCC or Clang) and CMake ≥ 3.25; the checked-in `CMakePresets.json` pins Ninja as
the generator. See `CLAUDE.md` for the full command reference, compiler flags, and toolchain notes.

## Documentation

- **[`CLAUDE.md`](CLAUDE.md)** — the condensed operating guide for working in this repository (architecture,
  commands, conventions, git workflow).
- **Specification** (`docs/specification/`) — the full architectural specification, one file per numbered
  section:

  1. [Vision](docs/specification/01-vision.md)
  2. [Core Principles](docs/specification/02-core-principles.md)
  3. [Architectural Definitions](docs/specification/03-architectural-definitions.md)
  4. [Architectural Invariants](docs/specification/04-architectural-invariants.md)
  5. [Dependency Model](docs/specification/05-dependency-model.md)
  6. [Server Authority](docs/specification/06-server-authority.md)
  7. [Host Composition](docs/specification/07-host-composition.md)
  8. [Atlas Hosts](docs/specification/08-atlas-hosts.md)
  9. [Capability Isolation and Previewing](docs/specification/09-capability-isolation-and-previewing.md)
  10. [The Editor Is a Client](docs/specification/10-the-editor-is-a-client.md)
  11. [Repository Layout](docs/specification/11-repository-layout.md)
  12. [Build Model](docs/specification/12-build-model.md)
  13. [Library Architecture](docs/specification/13-library-architecture.md)
  14. [Generated Contracts](docs/specification/14-generated-contracts.md)
  15. [Runtime Libraries](docs/specification/15-runtime-libraries.md)
  16. [Capability Libraries](docs/specification/16-capability-libraries.md)
  17. [Editor Libraries](docs/specification/17-editor-libraries.md)
  18. [Editor Extensions](docs/specification/18-editor-extensions.md)
  19. [UI System](docs/specification/19-ui-system.md)
  20. [Property and Resource Composition](docs/specification/20-property-and-resource-composition.md)
  21. [Worked Example](docs/specification/21-worked-example.md)
  22. [Incremental Compilation](docs/specification/22-incremental-compilation.md)
  23. [Platform Architecture](docs/specification/23-platform-architecture.md)
  24. [Non-Goals](docs/specification/24-non-goals.md)
  25. [Compatibility Philosophy](docs/specification/25-compatibility-philosophy.md)
  26. [Design Principle](docs/specification/26-design-principle.md)

- **[`demo/README.md`](demo/README.md)** — the growing gameplay demo built on top of Atlas, and the scope
  boundary it deliberately stays inside.
- **[`tools/atlas-cgen/README.md`](tools/atlas-cgen/README.md)** — the manifest-to-C++ contract generator.
- **Optional libraries** — `libraries/atlas-input/README.md`, `libraries/atlas-ui/README.md`,
  `libraries/atlas-render/README.md`, `libraries/atlas-audio/README.md`, `libraries/atlas-editor/README.md`
  each explain their current (not-yet-implemented) status.
