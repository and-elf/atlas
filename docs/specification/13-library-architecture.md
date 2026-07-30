## 13. Library Architecture

Atlas is organized as independent libraries.

```
atlas/
├── include/
├── src/
├── tools/
├── generators/
├── tests/
└── libraries/
    ├── atlas-contracts
    ├── atlas-core
    ├── atlas-entity
    ├── atlas-reflection
    ├── atlas-request
    ├── atlas-stage
    ├── atlas-scheduler
    ├── atlas-serialization
    ├── atlas-replication
    ├── atlas-resource
    ├── atlas-runtime
    ├── atlas-input
    ├── atlas-ui
    ├── atlas-render
    ├── atlas-audio
    └── atlas-editor
```

- Libraries expose stable public interfaces.
- Implementation remains private.
- Dependencies always point downward toward lower-level libraries.

### Library Responsibilities

Each library provides a focused architectural responsibility.

| Library | Provides |
|---|---|
| `atlas-contracts` | contract definitions, generated interfaces, public schema representation |
| `atlas-core` | foundational types, common utilities, platform primitives |
| `atlas-entity` | entity identity, entity lifecycle, entity management mechanisms |
| `atlas-reflection` | runtime metadata access, reflected structure discovery, tooling integration |
| `atlas-request` | request definitions, request execution infrastructure, request routing |
| `atlas-stage` | execution stages, lifecycle organization, deterministic ordering boundaries |
| `atlas-scheduler` | job scheduling, execution ordering, runtime coordination |
| `atlas-serialization` | serialization mechanisms, data encoding, persistence support |
| `atlas-replication` | state synchronization, replication mechanisms, network data distribution |
| `atlas-resource` | resource identity, resource resolution, resource management |
| `atlas-runtime` | host execution environment, runtime integration, coordination between systems |
| `atlas-input` | raw platform input polling, binding configuration, Intent event production; the sole source of `Intent` events entering the capability pipeline — raw key/button/axis data never crosses this boundary |
| `atlas-ui` | UI node tree, property binding infrastructure, behavior primitives (Clickable, Focusable, etc.), compositing layer management, backend dispatch |
| `atlas-render` | 3D rendering: consumes composed properties and resources, produces frame output; one possible backend for the UI renderer contract (§19), never the mandatory one |
| `atlas-audio` | audio rendering: consumes composed properties and resources, produces sound output; same mechanism-not-meaning boundary as `atlas-render` |
| `atlas-editor` | reusable editor capabilities, editor infrastructure, tooling integration |

The editor library remains optional. Gameplay applications do not depend on editor functionality. `atlas-input`, `atlas-ui`, `atlas-render`, and `atlas-audio` are similarly optional — a headless server host composes none of them.

### Capability Manifest

Every capability is declared by a single manifest — one YAML file per capability — covering both its structure (§14, Declarative Source Format: properties, requests, events, dependencies) and its identity as a build unit (source files, contract versions). Earlier sections of this document showed structural fields in isolation for brevity; the canonical, complete shape combines both under one `capability` block:

```yaml
capability:
  name: DamageResolution
  version: 1.4.0

depends_on:
  - entity
  - health

properties:
  ActiveResistance:
    physical: int32
    elemental: int32

requests:
  ResolveDamage:
    target: EntityRef
    instance: DamageInstance

events:
  DamageResolved:
    target: EntityRef
    final_amount: int32

source:
  root: src/
  files:
    - damage_resolution.cpp
    - modifiers.cpp
    - resistance.cpp
  includes:
    - include/

contracts:
  consumes:
    - id: atlas.combat.impact-event
      version: ^2.0
  produces:
    - id: atlas.combat.damage-event
      version: 1.0
```

- **`capability`** identifies the capability and its own version, following ordinary semantic versioning.
- **`depends_on`** declares the capability's dependencies at capability granularity, feeding the dependency graph and cycle detection described in §5. This is the sole mechanism by which execution order is determined — including broad ordering concerns like "presentation runs after simulation" (§5, Ordering Without Stages) — there is no separate stage or phase concept alongside it.
- **`properties`**, **`requests`**, **`events`** declare the capability's structural contract, exactly as described in §14 — generated into `constexpr` C++ contracts, never containing behavior (§14, The Declarative Boundary).
- **`source`** describes the build unit — which files and include paths Atlas tooling compiles as this capability's manual implementation (§14, Manual Implementation).
- **`contracts.consumes`** and **`contracts.produces`** declare the capability's position in the dependency graph in terms of the specific contracts it depends on and exposes, at a finer grain than `depends_on` alone — see Manifest Versioning vs. Contract Version Enforcement, below, for how this interacts with §5 and §6.

Examples elsewhere in this document that show only `depends_on` and a subset of structural fields (§19, §21) are abbreviated for readability — they omit `source` and `contracts` where the example's point doesn't depend on build/versioning detail, not because those fields are optional in a real manifest.

### Manifest Versioning vs. Contract Version Enforcement

The version ranges in a manifest's `consumes` block (e.g. `^2.0`) are a **build-time dependency resolution concern**, resolved by Atlas tooling when a host's capability graph is composed — the same role a package manager's version range plays when resolving a dependency graph before compilation. Tooling resolves each range to a single concrete contract version, validates the resulting graph against the same acyclic dependency rules already defined in §5, and fails the build if no compatible resolution exists.

This is a distinct concept from the contract version described in §6 (Contract Version Enforcement), and the two must not be conflated:

| | Manifest contract versions | §6 host contract version |
|---|---|---|
| **Scope** | Between capabilities, within a single build | Between a client host and a server host, at connection time |
| **Resolution** | Semantic version ranges, resolved by tooling at build time | Exact match only, checked at connection time |
| **Failure mode** | Compile-time build failure (§12, Compile-Time Validation) | Connection refused, with structured diagnostic (§6) |

A resolved build — the output of manifest-driven dependency resolution — produces exactly one concrete contract version for the host as a whole. It is *that* single resolved version §6 checks against a connecting client's, exact-match, with no ranges involved at that stage. Manifest version ranges give capability authors flexibility in what they depend on; §6's exact match gives runtime hosts certainty about what they're talking to. Loosening one does not loosen the other.
