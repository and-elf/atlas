# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Atlas is a compile-time composed, server-authoritative C++ platform for building real-time interactive applications (dedicated servers, gameplay clients, editors, automated test harnesses, CLI tools). It provides a deterministic runtime, compile-time capability composition, generated contracts, reflection, serialization, scheduling, replication, and resource identity — while deliberately avoiding any application semantics.

The full architectural specification lives at `docs/SPECIFICATION.md`. This file is a condensed operating guide; section references like `§4` point back into that document for authoritative detail — read the referenced section before making an architectural judgment call.

**This repository is a greenfield project.** At the time of writing it contains no source code yet. Treat the layout and conventions below as the target to establish, not an existing tree to discover by browsing.

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
├── tools/            # manifest validation, contract/reflection generation
├── generators/
├── tests/
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
    └── atlas-editor          # reusable editor capabilities (optional)
```

Before adding code, confirm which library it belongs in (§13 has the full responsibility table) rather than growing an unrelated one. `atlas-editor`, `atlas-input`, and `atlas-ui` are optional — a headless server host must never gain a dependency on any of them (§13).

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

- **Language standard: C++23** everywhere (`-std=c++23` / `/std:c++23`). Prefer standard-library facilities (concepts, ranges, `std::expected`) over hand-rolled equivalents — Atlas's own tiny-interface contract checking is built on concepts (§5, §20).
- **Build system: CMake**, one `CMakeLists.txt` per library under `libraries/`, preferring `target_compile_features`/interface targets over global compiler flags.
- **Compiler flags** (GCC and Clang), applied to every target, warnings treated as errors:
  - `-std=c++23 -Wall -Wextra -Wpedantic -Werror`
  - `-Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough`
  - Debug/sanitizer builds add: `-fsanitize=address,undefined -fno-omit-frame-pointer`
  - Never add `-ffast-math` or any flag relaxing IEEE-754 semantics; keep `-ffp-contract=off` explicit rather than relying on the compiler default — this is a direct consequence of the determinism guarantee (§4), not an arbitrary style choice.
- **clang-format** is enforced, not a suggestion — every file must be clang-format-clean before commit. Run `clang-format -i` on changed files; both the pre-commit hook and CI reject unformatted diffs (see Hooks below).
- **clang-tidy** runs as part of the build and in CI, warnings treated as errors. Enable at minimum `bugprone-*`, `cppcoreguidelines-*`, `performance-*`, and `modernize-*`; fix the issue or add a `NOLINT` with a stated reason rather than disabling a check globally.

## Testing & Coverage

- **Strict TDD (red-green-refactor)**: write the failing test first, confirm it fails for the expected reason, implement the minimum to pass, then refactor. This applies to capability request handlers, composition strategies, and runtime library code alike.
- Because every host is just a capability composition (§9), test a capability by composing it into a minimal test host — not by mocking out its behavior. A bug reproduced in a test host is a real bug in the same code the game runs; there is no separate "test approximation" of the runtime.
- **Code coverage gate: 75% minimum** (line/branch), enforced in CI. A change that drops coverage below the gate fails the pipeline the same way a failing test would.
- Determinism is itself testable: given the same seed and recorded input stream, asserting bit-exact repeated output across runs is a valid and expected category of test in this codebase (§4).

## Hooks (mandatory)

Per org policy this repository must have these wired as actual Git hooks, not left to individual discipline:

- **gitleaks** — pre-commit secret scanning; a commit containing what looks like a credential is blocked, not just flagged.
- **clang-format / clang-tidy** — pre-commit or pre-push check matching CI exactly, so violations are never caught only in the pipeline.

## Git Workflow

- `.gitignore` is already present — extend it as new build/tooling artifacts appear rather than replacing it.
- Never push to a remote, or open/merge a PR, without explicit user confirmation.
- When new requirements come in: update `docs/SPECIFICATION.md` first (flag conflicts with the user), then tests, then code, then verify against spec + tests, then propose a commit message / PR before taking any repository action.
- Verify the CI pipeline is green before merging any PR into the default branch.
- Do not reference AI/Claude authorship in commit messages, code comments, or docs.
