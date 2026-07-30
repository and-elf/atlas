# demo

A growing gameplay demo built on top of Atlas — proving out the platform's mechanisms (request dispatch,
property composition, replication, and more as they're built) against real, if minimal, gameplay capabilities,
rather than each library's own isolated unit tests alone.

**This is staged here temporarily, not a permanent part of the Atlas platform.** `CLAUDE.md` is explicit that
Atlas itself "never understands players, health, weapons, inventories, quests, or game rules" — the capabilities
under `modules/` (`health`, `armor`, `equipment`, and more as the demo grows) are gameplay semantics, not
platform code. They live in this repo for now because it's the fastest way to prove new platform mechanisms
against something real as they're built; the plan is to move this directory into its own repository once it's
grown into something suitable and stable enough to stand alone (an `external/atlas/`-style consumer, matching
spec §11's "project consuming Atlas" layout) — `modules/` is already structured that way today so that move is a
lift-and-shift, not a rewrite.

## Structure

```
demo/
├── CMakeLists.txt
├── README.md
├── modules/            # one directory per capability - manifest + hand-written implementation
│   ├── health/
│   │   ├── health.capability.yaml
│   │   ├── health.hpp
│   │   └── health.cpp
│   ├── armor/
│   │   ├── armor.capability.yaml
│   │   ├── armor.hpp
│   │   └── armor.cpp
│   └── equipment/
│       ├── equipment.capability.yaml
│       ├── equipment.hpp
│       └── equipment.cpp
└── tests/
    ├── simulated_host.hpp        # shared test scaffolding (SimulatedHost) - not a capability
    ├── combat_scenario_test.cpp
    ├── equipment_test.cpp
    ├── health_test.cpp
    └── armor_test.cpp
```

Each module's manifest is generated into a real, compiling C++ contract via `atlas-cgen` (`cmake/GenerateCapabilityContract.cmake`,
the same shared helper `tests/atlas-cgen`/`tests/atlas-contracts` use), and its `.hpp`/`.cpp` is the hand-written
manual implementation a capability author writes against that contract (spec §14).

## The combat scenario

**Scenario:** client A issues `ApplyDamage(target=B, amount=10)`. B has 10 `Health` and an `Armor` property
composed (Additive, spec §20) from a single +5 contribution. The server validates authority, resolves `Armor`'s
effective value through composition, computes `10 - 5 = 5` effective damage, mutates `Health` to 5, and
replicates the result — over a genuinely serialized wire, not shared memory — to both client A and client B.
Both clients read `Health.current == 5`.

### What this proves, concretely

- **Request routing + validation** (`atlas-request`'s `Dispatcher<T>`, `atlas-runtime`'s `Context`): a request
  reaches its handler, the handler checks `ctx.host().has_authority()`, and rejects or accepts accordingly.
- **Property composition** (`atlas-contracts`' `Composable<T>`, `atlas-runtime`'s `resolve_additive`): `Armor`'s
  effective value is a real Additive resolution over a tracked contribution, not a hard-coded number — and the
  `health` capability that reads it never knows `Armor` is composed at all, only that `ctx.get<Armor>(target)`
  returns *some* value (spec §20's Design Rule: "Capability B consumes the effective value; it never needs to
  know who contributed to it").
- **Property replication**: the server's `Health` value is genuinely serialized (`health::write_health`, built
  on `atlas-serialization`'s `write_i32`) and decoded (`health::read_health`) on each client — not copied by
  reference, not shared in memory. Both clients independently decode the identical bytes to the identical
  value.

## Equipping gear

`equipment`'s `EquipArmor` request contributes to `Armor` through the same channel `armor::add_contribution`
already exposes — the sanctioned way to affect another capability's composed property (spec §20, Design Rule:
"A capability must not directly modify another capability's state. Contribution is the only channel"). This
replaces `combat_scenario_test.cpp`'s test-harness-injected contribution with a real request path for a second
scenario (`equipment_test.cpp`): equip two items (accumulating additively), then attack, and the mitigation
reflects what was actually equipped — not a value the test wired in directly.

`equipment` and `health` relate to `armor` differently, and both are legitimate:

- **`health` reads Armor's contract** (`ctx.get<armor::Armor>(target)`) — a tiny-interface dependency (spec §5)
  on another capability's property *shape*. It never knows Armor is composed, only that reading it returns a
  value.
- **`equipment` contributes to Armor** (`armor::add_contribution(...)`) — a declared dependency on Armor's
  *contribution* channel, not its internal state. Neither capability reaches into the other's `PropertyStore`
  directly either way.

`EquipArmor.bonus` is carried directly on the request (like `ApplyDamage.amount`), not looked up from the
item's `ResourceId` via some item-stats table — that lookup would need resource *compilation* (`atlas-rcc`,
not yet built), which doesn't exist yet (see `tools/atlas-cgen/README.md`'s "Contract generation vs. resource
compilation" section). `armor::Contribution::source` stays a fixed
`"equipment"` label rather than one derived per-item from `cmd.item`, since `Contribution::source` is a
non-owning `std::string_view` (see `atlas-runtime/property_composition.hpp`) — a per-item label would need
owned storage nothing currently requires; the trigger to widen it would be a real need to remove one specific
equipped item's contribution by name, which doesn't exist yet either.

## What this deliberately does *not* build (and why)

Building all of §7/§8/§20/§6 in full, in one round, would be a separate epic on its own — each of the following
is a real, sizable feature this demo intentionally stays inside a smaller boundary around, for now:

- **No manifest-driven capability composition.** A real Atlas host is assembled by tooling from capability
  manifests, resolving `depends_on` into a graph (§7, §8). That generator doesn't exist yet. `SimulatedHost`
  (in `tests/combat_scenario_test.cpp`) hand-composes capabilities into a host directly in C++ — the same scope
  boundary `atlas-runtime`'s own `Host` already draws around itself ("the hand-composed runtime substrate such a
  manifest-driven host would eventually sit on top of").
- **Only one composition strategy (Additive).** Spec §20 names seven; only Additive has a working evaluator
  (`atlas::runtime::resolve_additive`). The other six (Multiplicative, Override, Priority Override, Set Union,
  Ordered Composition, Weighted Composition) each have genuinely different resolution semantics that don't
  generalize from Additive, and are each their own future increment — `movement`'s `MovementSpeed` will need
  Multiplicative when it's built.
- **No standing, general-purpose contribution registry.** `atlas::armor::ContributionRegistry` is `armor`'s own
  private per-entity bookkeeping (owned per-host, passed explicitly to `add_contribution` — never a namespace-
  scope global, which would silently collide between independently-created `Host` instances allocating entity
  indices from 0 each). A real capability system would likely want a generic version of this in `atlas-runtime`
  itself; building that generalization ahead of a second real use case would be speculative.
- **No client-side prediction or reconciliation** (spec §6). Client A, in this demo, simply waits for the
  server's replicated result — it never locally guesses `Health`'s outcome before the server confirms it.
- **No real network transport.** Hosts talk in-process (spec §7: "Host Communication... in-process calls... test
  harness integration" are all legitimate), but the wire *encoding* itself is real (see Property replication
  above) — this is not a shortcut around serialization, only around actual sockets/connections.
- **Health's wire encoding is hand-written, not generic.** `atlas-reflection`'s `for_each_field` makes a
  reflection-driven generic property codec genuinely buildable now — but this demo hand-writes
  `write_health`/`read_health` directly on `atlas-serialization` primitives, mirroring `atlas-replication`'s
  existing `EntityRef`/`ResourceId` codec precedent. A generic version is a great, now-unblocked next step, not
  attempted here.
- **`RequestResult` chaining uses `.transform(...).value_or(...)`, not spec §21's own `.or_else(...).and_then(...)`
  pseudocode.** Spec §21 explicitly flags its own code as "illustrative pseudocode, not a literal Atlas API
  surface." `std::optional<T>::or_else`/`and_then` (C++23) both require the callback to return another
  `std::optional` of the same value type — chaining directly into a `RequestResult` return (a different type)
  doesn't type-check that way. `.transform(...)` (mapping to a `RequestResult` on the success path)
  `.value_or(...)` (supplying the rejection on the empty path) is the real, compiling C++23 shape of the same
  monadic idea.

## Fixture duplication note

`modules/health/health.capability.yaml` and `modules/armor/armor.capability.yaml` are this demo's own copies,
distinct from the stable `tests/fixtures/health.capability.yaml` / `tests/fixtures/armor.capability.yaml` that
`atlas-cgen`'s and `atlas-contracts`' own test suites use to prove the generator itself. This demo's copies are
free to evolve (gain fields, change `depends_on`) as the demo grows, without that evolution rippling into
unrelated generator/contract tests that need a small, stable, unchanging fixture.
