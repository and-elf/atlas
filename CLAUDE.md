# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Atlas is a compile-time composed, server-authoritative C++ platform for building real-time interactive applications (dedicated servers, gameplay clients, editors, automated test harnesses, CLI tools). It provides a deterministic runtime, compile-time capability composition, generated contracts, reflection, serialization, scheduling, replication, and resource identity — while deliberately avoiding any application semantics.

The full architectural specification lives at `docs/specification/` (see the root [`README.md`](README.md) for the section index), split one file per numbered section — `§4` is `docs/specification/04-architectural-invariants.md`, `§13` is `13-library-architecture.md`, and so on. This file is a condensed operating guide; section references like `§4` point back into that directory for authoritative detail — read the referenced section before making an architectural judgment call.

**This repository is early-stage.** The build/lint/test/coverage scaffolding is in place and enforced end-to-end, seeded with eleven real libraries (`atlas-core`, `atlas-entity`, `atlas-contracts`, `atlas-stage`, `atlas-resource`, `atlas-serialization`, `atlas-reflection`, `atlas-request`, `atlas-scheduler`, `atlas-replication`, `atlas-runtime`) plus a first tool, `tools/atlas-cgen` (manifest-to-C++ contract generator, scoped to reproducing §21's worked example — see its README for exactly what it does and doesn't do yet). Only the five optional libraries (`atlas-input`, `atlas-ui`, `atlas-render`, `atlas-audio`, `atlas-editor`) remain unimplemented — each has a status-stub `README.md` under `libraries/<name>/` instead. Treat "Repository Layout" below as the target to grow into, not an already-populated tree.

## Commands

```sh
# One-time, per clone: wire up the mandatory hooks (see Hooks below)
git config core.hooksPath .githooks

# Configure + build + test (presets defined in CMakePresets.json)
cmake --preset debug && cmake --build --preset debug && ctest --preset debug   # sanitized debug (default day-to-day loop)
cmake --preset release && cmake --build --preset release && ctest --preset release

# Run a single test binary / a single test case
build/debug/tests/atlas-core/atlas-core-tests
build/debug/tests/atlas-core/atlas-core-tests --gtest_filter='SemanticVersion.OrdersByPrecedence'

# Format (matches CI exactly; run before committing)
clang-format -i $(git ls-files '*.cpp' '*.hpp')
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')   # check only, no rewrite

# Static analysis (clang-tidy runs per translation unit as part of the build)
cmake --preset clang-tidy && cmake --build --preset clang-tidy

# Coverage gate (75% line + branch; fails the build if under)
cmake --preset coverage && cmake --build --preset coverage && ctest --preset coverage
cmake --build build/coverage --target coverage   # report at build/coverage/coverage/index.html
```

Adding a library follows the `atlas-core` pattern: a `CMakeLists.txt` defining the target and linking `atlas_project_options`/`atlas_project_warnings`, headers under `include/atlas/<name>/`, sources under `src/`, and a matching `tests/<name>/` directory added to `tests/CMakeLists.txt`.

## Core Architectural Model (§1–§4)

Three principles govern everything in Atlas:

- **Atlas defines execution. Capabilities define behavior. Applications define meaning.** (Mechanism Over Meaning, §2) — the runtime understands entities, properties, requests, events, stages, jobs, resources, scheduling. It never understands players, health, weapons, inventories, quests, or game rules.
- **All composition happens at compile time** (§4). There is no plugin discovery and no runtime capability registration. The build fails on an invalid dependency graph, contract, or stage ordering — this is a hard compile-time error, not a runtime discovery failure (§12).
- **Bit-exact determinism is a hard guarantee**, not best-effort: identical inputs produce identical outputs down to the bit, across machines and across full-session replay (§4). This has direct code-level consequences — see "Determinism Constraints" below.

Key vocabulary (§3):

| Term | Meaning |
|---|---|
| Runtime | Shared, stable execution environment. Never contains application logic. |
| Tooling | Compile-time validation/generation (contracts, reflection, docs) run during the build. |
| Capability | Compile-time composable unit of behavior (entities, properties, requests, events, systems, stages, jobs). |
| Contract | Generated public interface — `constexpr` structure, never behavior. |
| Host | A logical execution context (server, client, editor, test harness, CLI tool) composed from capabilities. Not an OS process. |
| Resource | Externally authored asset resolved by stable identity, never a hard-coded path, scoped to what the composing host actually references. |

## Dependency Rules (§5)

Dependencies only point downward: `Application → Capability Libraries → Generated Contracts → Runtime Libraries → Platform Services`. An upward dependency (e.g. a runtime library depending on a capability) is always wrong.

- Capabilities may depend only on lower-level capabilities, generated contracts, and runtime libraries — never on applications, editor implementations, or deployment-specific code.
- Capability ordering is derived **entirely** from the `depends_on` graph declared in each capability's manifest. There is no separate stage/tier/phase concept sitting alongside it — this includes ordering concerns that look like they need one, e.g. "presentation runs after simulation" falls out of the graph alone (§5, Ordering Without Stages).
- A dependency cycle is an invalid composition and a hard compile-time build failure; tooling must report the full chain of edges forming the cycle, not just the first one found.
- Capabilities compose through small, single-purpose ("tiny") contracts rather than depending on a whole entity or resource — e.g. depend on a `HasPosition`-shaped contract, not the entity's full property set. Contract satisfaction is a compile-time fact (`constexpr`, checked like a C++ concept), never a runtime interface table or virtual dispatch lookup.
- Raw platform input never crosses into capability code — `atlas-input` resolves it into semantic `Intent` events first (§5, Input as Intent). A capability author never sees "was E pressed," only "did the player express this intent."

## Determinism Constraints (§4) — code-level implications

Because Atlas guarantees bit-exact determinism, code in runtime/capability libraries must:

- **Never read platform wall-clock time or OS/hardware entropy directly** inside simulation logic. Use Atlas's built-in deterministic `Time` and `Random` types only — the per-host seeded random stream is the sole permitted randomness source. Wall-clock time may be used only for presentation-only concerns (audio/render interpolation) and must never feed back into simulation state.
- **Never enable compiler flags that relax IEEE-754 floating-point semantics** (e.g. `-ffast-math`) — FP results must not vary by platform or instruction set.
- **Avoid unordered iteration** over concurrent/parallel work anywhere it could affect simulation state.
- **Never leave memory uninitialized** where it could influence output.

Treat any of the above found in review as an architectural defect, not a style nitpick — that is how §4 explicitly frames it.

## Capability & Contract Authoring (§12–§14, §20)

- A capability's **structure** (properties, requests, events, dependencies, composition strategy) is authored declaratively as YAML — one manifest per capability, under a `capability:` block, following the shape in §13 (Capability Manifest). Manifests also declare `source:` (which files/includes the capability compiles) and `contracts:` (`consumes`/`produces`, with semver ranges resolved at build time — distinct from the exact-match host contract version checked at connection time, §13/§6).
- The declarative format expresses **structure and composition parameters only** — never conditionals, branching, or per-tick logic. The moment a YAML file needs an `if`, that logic belongs in a `.cpp` file instead (§14, The Declarative Boundary).
- Atlas tooling consumes manifests and **generates** `constexpr` C++ contracts (structs plus `static_assert(atlas::PropertyContract<T>)` / `RequestContract<T>` / `EventContract<T>`). Generated files carry a `do not hand-edit` header — never edit them; change the manifest instead.
- **Manual implementation is always hand-written C++**, placed in the `.cpp` files declared under the manifest's `source:` block, expressing behavior against the generated contract via the typed, monadic context API (`ctx.get<T>()`, `ctx.publish<T>()`) — see §21 for a complete worked example.
- A **request** is the only dispatchable contract kind. "Command" is not a separate schema — it's descriptive terminology for a request dispatched internally, capability-to-capability, rather than one issued across the client/server boundary (§6, Terminology: Request vs. Internal Dispatch).

## Server Authority & Requests (§6)

- Authoritative simulation runs only on server hosts; client hosts observe replicated state and issue requests. A request handler must validate against authoritative state and explicitly reject invalid requests — never silently coerce a request into something valid.
- Clients may predict a request's outcome locally and reconcile (resimulate) if the server's actual outcome differs; reconciliation is a **capability** concern, not something the runtime does for you.
- Atlas does not enforce *who* may issue a request — only that origin metadata is authentic. Permission/trust checks are the capability's own responsibility, enforced as part of the same validation step (an unauthorized request is simply an invalid request).
- Runtime failures (resource resolution, replication delivery, disconnects, request rejection, contract version mismatch) all report through one uniform event channel — don't invent a per-system error convention.

## Property Composition (§20)

Multiple, mutually-unaware capabilities may contribute to one property (e.g. `MovementSpeed`) via a fixed set of composition strategies: Additive, Multiplicative, Override, Priority Override, Set Union, Ordered Composition, Weighted Composition. A capability must never reach into another capability's state directly — contributing through the property system is the only channel. When adding a new composed value, pick the strategy matching its semantics rather than inventing ad-hoc merge logic. Resources (`ParticleEffects`, `MaterialLayers`, etc.) compose the same way properties do — same mechanism, not a special case.

## Repository Layout (target)

Atlas itself:

```
atlas/
├── CMakeLists.txt
├── include/
├── src/
├── tools/            # manifest validation, contract/reflection generation — tools/atlas-cgen/ is seeded
├── generators/       # not yet used — deferred until a second generator/backend exists to justify a separate namespace from tools/
├── tests/
├── demo/             # growing gameplay demo built on Atlas, staged here temporarily — see demo/README.md
└── libraries/
    ├── atlas-contracts       # contract definitions, generated interfaces
    ├── atlas-core            # foundational types, common utilities, platform primitives
    ├── atlas-entity          # entity identity, lifecycle, management
    ├── atlas-reflection      # runtime metadata access, tooling integration
    ├── atlas-request         # request definitions, execution infra, routing
    ├── atlas-stage           # execution stages, deterministic ordering boundaries
    ├── atlas-scheduler       # job scheduling, execution ordering
    ├── atlas-serialization   # encoding, persistence support
    ├── atlas-replication     # state synchronization, network data distribution
    ├── atlas-resource        # resource identity, resolution, management
    ├── atlas-runtime         # host execution environment, system coordination
    ├── atlas-input           # raw input polling, binding config, Intent production (optional)
    ├── atlas-ui              # UI node tree, property binding, behaviors (optional)
    ├── atlas-render          # 3D rendering: composed state in, frame output out (optional)
    ├── atlas-audio           # audio rendering: composed state in, sound output out (optional)
    └── atlas-editor          # reusable editor capabilities (optional)
```

Before adding code, confirm which library it belongs in (§13 has the full responsibility table) rather than growing an unrelated one. `atlas-editor`, `atlas-input`, `atlas-ui`, `atlas-render`, and `atlas-audio` are optional — a headless server host must never gain a dependency on any of them (§13).

**`demo/` is not part of the Atlas platform** — Atlas "never understands players, health, weapons, inventories, quests, or game rules," and `demo/`'s capabilities (`modules/health`, `modules/armor`, `modules/equipment`, and more as it grows) are gameplay semantics, not platform code. It's staged inside this repo temporarily because that's the fastest way to prove new platform mechanisms (request dispatch, property composition, replication, ...) against something real as they're built, rather than each library's isolated unit tests alone. It's structured like §11's "project consuming Atlas" layout (`modules/` per capability, its own `tests/`) specifically so the eventual move to its own repository, once it's grown into something suitable and stable enough to stand alone, is a lift-and-shift rather than a rewrite. See `demo/README.md` for what it currently proves and the scope boundary it deliberately stays inside.

**Naming convention: every platform tool under `tools/` is prefixed `atlas-`** (`atlas-cgen`, the manifest-to-C++ contract generator; `atlas-rcc`, the not-yet-built resource compiler; more candidates below), the same convention libraries already follow. A tool's C++ namespace matches its suffix (`atlas::cgen` for `atlas-cgen`, mirroring `atlas::core` for `atlas-core`). Spec §12 (Atlas Tooling) lists several distinct compile-time responsibilities that are separate tools, not modes of one monolithic tool, each earning its own `atlas-` name as it gets built: contract generation (`atlas-cgen`, implemented), resource compilation — turning authored asset lists into compiled `ResourceId` tables (`atlas-rcc`, not yet built), reflection metadata generation, capability dependency-graph validation (the cycle-detection compile-time failure required by §5), and documentation generation. A generator understanding a vocabulary *type* (e.g. `atlas-cgen` mapping a manifest field typed `ResourceId` to `atlas::ResourceId` and its header) is not the same responsibility as *compiling* that type's backing data — don't conflate the two when deciding which tool a piece of logic belongs in.

A project *consuming* Atlas (rather than Atlas itself) instead looks like (§11):

```
MyGame/
├── CMakeLists.txt
├── atlas.project
├── external/atlas/     # Atlas as a git submodule / source dependency
├── modules/
├── src/
├── resources/
├── config/
├── tests/
└── generated/          # tooling output — never hand-edit, never treat as source of truth
```

## Build & Toolchain

- **Language standard: C++23** everywhere (`-std=c++23` / `/std:c++23`, set via `CMAKE_CXX_STANDARD` in the root `CMakeLists.txt`). Prefer standard-library facilities (concepts, ranges) over hand-rolled equivalents — Atlas's own tiny-interface contract checking is built on concepts (§5, §20). **Exception: avoid `std::expected` for now.** Verified (not hypothetical) that Clang 18 + libstdc++ — this project's own CI configuration, not a hypothetical toolchain — cannot compile it: libstdc++'s `<expected>` gates its entire contents behind `__cpp_concepts >= 202002L`, and Clang reports `201907L`, so the header silently declares nothing. Throw `std::invalid_argument` (or another appropriate `<stdexcept>` type) for fallible operations instead — see `tools/atlas-cgen/README.md` for the full writeup. Revisit once Clang's `__cpp_concepts` value catches up.
- **Third-party `FetchContent` dependencies need `SYSTEM`** (CMake ≥ 3.25, e.g. `FetchContent_Declare(yaml-cpp SYSTEM ...)`): without it, a warning purely inside the dependency's own headers can still fail our project-wide `-Werror`, because `.clang-tidy`'s `HeaderFilterRegex` only suppresses clang-tidy's own check diagnostics for third-party headers, not the plain compiler diagnostics clang-tidy also surfaces.
- **Rule of Zero.** A plain value type with no invariant to protect (e.g. `atlas::core::SemanticVersion`, `atlas::EntityRef`) is a basic aggregate: public fields with default member initializers, no hand-written constructor, no private state — comparison operators default (`= default`) rather than being hand-rolled. Reach for an encapsulated class only when a type actually has an invariant to protect across its own operations (e.g. `atlas::entity::EntityRegistry`'s slot/free-list consistency) — don't add private members + getters "for encapsulation's sake" where a struct would do.
- **Build system: CMake**, generator-agnostic in principle but the checked-in `CMakePresets.json` pins **Ninja** everywhere (needed so `CMAKE_BUILD_TYPE` actually means something on Windows, where the default generator is multi-config and ignores it). One `CMakeLists.txt` per library under `libraries/`; flags come from the shared `atlas_project_options`/`atlas_project_warnings` interface targets (`cmake/*.cmake`), not per-target `-W` flags.
- **Compiler flags** (GCC and Clang), applied to every target via `atlas_project_warnings`, warnings treated as errors by default (`ATLAS_WARNINGS_AS_ERRORS`):
  - `-std=c++23 -Wall -Wextra -Wpedantic -Werror`
  - `-Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough -Wcast-align -Wunused`
  - Sanitized builds (`ATLAS_ENABLE_SANITIZERS`, on by default in the `debug` preset) add: `-fsanitize=address,undefined -fno-omit-frame-pointer`
  - Every target gets `-ffp-contract=off` unconditionally (`cmake/CompilerWarnings.cmake` / root `CMakeLists.txt`). Never add `-ffast-math` or any flag relaxing IEEE-754 semantics — this is a direct consequence of the determinism guarantee (§4), not an arbitrary style choice.
- **clang-format** (`.clang-format`, LLVM-based, 110-column) is enforced, not a suggestion — every file must be clang-format-clean before commit. Run `clang-format -i` on changed files; both the pre-commit hook and CI's `format` job reject unformatted diffs (see Hooks below).
- **clang-tidy** (`.clang-tidy`: `bugprone-*`, `cppcoreguidelines-*`, `performance-*`, `modernize-*`, `readability-*`), with `--warnings-as-errors=*` — fix the issue or add a `NOLINT(check-name)` with a one-line reason rather than disabling a check globally. `tests/.clang-tidy` disables only `bugprone-unchecked-optional-access` for test code (a documented false positive with GoogleTest's `ASSERT_TRUE`/`ASSERT_FALSE` macros, not a relaxed bar). Third-party targets (e.g. fetched GoogleTest) are explicitly exempted per `tests/CMakeLists.txt` — never let the gate reach into a dependency's source.
  - `cmake --preset clang-tidy && cmake --build --preset clang-tidy` (`-DATLAS_ENABLE_CLANG_TIDY=ON`, the preset's default) runs it per translation unit as part of the build — a full manual sweep of the whole tree, for a deliberate local audit.
  - The automated gates (pre-push hook, CI's `static-analysis` job) don't do that full sweep — they configure with `-DATLAS_ENABLE_CLANG_TIDY=OFF` for a fast plain build (still producing `compile_commands.json`), then run `cmake/scripts/clang-tidy-changed-files.sh` to invoke clang-tidy directly against only the `.cpp` files that actually changed relative to the push's previous state (or the PR's base ref in CI). Checking every tracked source file on every push stopped scaling once the codebase grew past its first few libraries — this is the fix, not a relaxed bar: the same `--warnings-as-errors=*` check runs, just scoped to what changed.

## Testing & Coverage

- **Test framework: GoogleTest** (`FetchContent`-fetched, pinned to a tag in `tests/CMakeLists.txt`; `gtest_discover_tests` registers cases individually with CTest). `libraries/atlas-core` + `tests/atlas-core` is the reference pattern for adding a new library and its tests.
- **Strict TDD (red-green-refactor)**: write the failing test first, confirm it fails for the expected reason, implement the minimum to pass, then refactor. This applies to capability request handlers, composition strategies, and runtime library code alike.
- Because every host is just a capability composition (§9), test a capability by composing it into a minimal test host — not by mocking out its behavior. A bug reproduced in a test host is a real bug in the same code the game runs; there is no separate "test approximation" of the runtime.
- **Code coverage gate: 75% minimum, line AND branch** (`ATLAS_COVERAGE_THRESHOLD` in `cmake/CodeCoverage.cmake`), enforced by the `coverage` CMake target (`gcovr --fail-under-line --fail-under-branch`) and CI's `coverage` job. A change that drops coverage below the gate fails the pipeline the same way a failing test would — the fix is to add the missing test case, not to lower the threshold. Branch coverage in particular tends to expose untested error paths (e.g. malformed-input handling) that line coverage alone would miss.
  - The gate runs with `--exclude-throw-branches` (gcovr's own purpose-built option): compiler-generated exception-unwind edges from heavy `std::string`/`std::vector` use are not testable application logic. Before ever reaching for this, confirm with `gcov -b` directly on the `.gcda` file that every genuinely-unexecuted *line* (not just under-covered branch) already has a real test — `tools/atlas-cgen`'s history is the worked example of doing this investigation properly rather than reaching for the flag first.
  - `tools/*/src/main.cpp` (CLI entry points) are excluded from the gate — argv/file-I/O error paths need subprocess-spawning test infrastructure this project doesn't build yet. The library logic a CLI wires together should still be fully unit-tested underneath it.
- Determinism is itself testable: given the same seed and recorded input stream, asserting bit-exact repeated output across runs is a valid and expected category of test in this codebase (§4).

## Hooks (mandatory)

Per org policy this repository has these wired as actual Git hooks (`.githooks/`, enabled via `git config core.hooksPath .githooks` — see Commands), not left to individual discipline:

- **pre-commit** — `gitleaks protect --staged` (blocks a commit containing what looks like a credential) and `clang-format --dry-run --Werror` on staged C++ files.
- **pre-push** — `clang-tidy --warnings-as-errors=*`, scoped to only the `.cpp` files changed in the commits being pushed (`cmake/scripts/clang-tidy-changed-files.sh`, diffed against each ref's previous remote SHA), against whatever configured build (`build/*/compile_commands.json`) it finds locally — matching CI's `static-analysis` job, which applies the same changed-files scoping against the PR's base ref (or the previous push's SHA on a direct push). If no build is configured yet it warns and skips rather than blocking the push — CI is still the authoritative gate either way.

## Git Workflow

- `.gitignore` is already present — extend it as new build/tooling artifacts appear rather than replacing it.
- CI (`.github/workflows/ci.yml`) runs on every push/PR: `gitleaks` → `format` → (`static-analysis`, `build-and-test` matrix: gcc/clang debug-sanitized + release on Linux, release on macOS/Windows) → `coverage`. All of it is reachable locally through the commands and hooks above — nothing in CI should be the first place a violation is seen.
- Never push to a remote, or open/merge a PR, without explicit user confirmation.
- **Issue-first workflow (standard from here on)**: before implementing any change — a new capability, a refactor, a tooling feature, anything beyond a trivial doc fix — open a GitHub issue describing it first. Implement against that issue, then open the pull request that closes it (e.g. a `Closes #N` line in the PR body). Work is issue → implementation → PR, in that order, not the other way around.
- When new requirements come in: update the relevant `docs/specification/*.md` file(s) first (flag conflicts with the user), then tests, then code, then verify against spec + tests, then propose a commit message / PR before taking any repository action.
- Verify the CI pipeline is green before merging any PR into the default branch.
- Do not reference AI/Claude authorship in commit messages, code comments, or docs.
