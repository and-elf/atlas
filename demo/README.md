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
│   ├── equipment/
│   │   ├── equipment.capability.yaml
│   │   ├── equipment.hpp
│   │   └── equipment.cpp
│   ├── movement/
│   │   ├── movement.capability.yaml
│   │   ├── movement.hpp
│   │   └── movement.cpp
│   └── pathing/
│       ├── pathing.capability.yaml
│       ├── pathing.hpp
│       └── pathing.cpp
└── tests/
    ├── simulated_host.hpp        # shared test scaffolding (SimulatedHost) - not a capability
    ├── combat_scenario_test.cpp
    ├── equipment_test.cpp
    ├── health_test.cpp
    ├── armor_test.cpp
    ├── movement_test.cpp
    ├── healing_test.cpp
    └── pathing_test.cpp
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

## Moving around

`movement` introduces the platform's second composition strategy, Multiplicative, for `MovementSpeed` —
reproducing spec §20's own worked example directly (`movement_test.cpp`'s `SpeedContributionsComposeMultiplicatively`
and `MoveAdvancesPositionByComposedSpeedOverElapsedTicks`): a base speed of `10.0`, a `"slow"` contribution of
`0.5`, and a `"haste"` contribution of `1.2`, folding to an effective speed of `6.0`. A `Move` request then
advances `Position` along a caller-supplied direction at that effective speed, over a caller-supplied
`delta_ticks` — deterministic simulation ticks (`atlas::core::Time::ticks_per_second`), never wall-clock time
(spec §4).

**Why `movement` can't reuse `armor::add_contribution`'s pattern as-is.** `armor::add_contribution` hardcodes
`0` as Additive's resolution starting point on every call, which only works because `Armor`'s declared base
happens to already equal Additive's identity value (`0`) — atlas-cgen doesn't parse per-field default-value
literals yet, so an unset `base` is always `0` regardless of composition strategy. Multiplicative's identity is
`1.0`, not a property's actual declared base (`MovementSpeed`'s is a real, non-identity `10.0` in the scenario
above) — resolving from a hardcoded `1.0` would silently discard the real base on every contribution. So
`movement::ContributionRegistry` tracks a richer `SpeedContributions{declared_base, contributions}` per entity
instead of armor's bare `vector<Contribution<T>>`: `set_base_speed` records the declared base once (and seeds
the initial effective value, since a base with zero contributions degenerates to itself), and
`add_speed_contribution` always resolves from that tracked `declared_base` — never from
`PropertyStore<MovementSpeed>`'s current value, which by definition already holds the *previous* resolution's
output rather than the original declared value.

**`movement::remove_speed_contribution` is the reverse of `add_speed_contribution`** — laying groundwork for a
future aura mechanism, not yet consumed by anything in this demo. Every contribution added so far (`armor`,
`movement`) has been `Permanent`: added once, never removed. An aura's contribution is different — it should
stop applying the instant its own governing condition fails (e.g. the target leaves the source's range, spec
§20's `WhileCondition` lifetime), which needs an actual removal path nothing had until now. `remove_speed_contribution`
mirrors `add_speed_contribution` exactly: it re-resolves the effective value from the entity's tracked
`declared_base` and whatever contributions remain, never by trying to algebraically "undo" the removed factor
against the previous effective value (`RemoveSpeedContributionUndoesItsMultiplier` proves this — removing
`"slow"` from `10 x 0.5 x 1.2 = 6.0` gives `10 x 1.2 = 12.0`, a fresh resolution, not `6.0 / 0.5`). Built on
`atlas::runtime::remove_contributions_by_source<T>`, a new generic (non-strategy-specific) primitive: it only
erases matching entries and reports how many, leaving resolution to the caller exactly like
`add_speed_contribution` already does — the same reason `resolve_additive`/`resolve_multiplicative` stay separate
functions rather than one that also decides which strategy applies.

## Healing is signed damage

Healing is not its own mechanism, capability, or request. `health::ApplyDamage.amount` is already `int32_t` -
signed - so a positive amount is damage (unchanged from the combat scenario above: Armor mitigates it, spec
§20) and a negative amount is healing, applied straight to `Health.current`. There is no separate `Heal`
request and no `on_heal` handler; `on_apply_damage` is the only request-handler code path for both directions,
because the two only ever differed in the sign of one field, not in what mechanism moves `Health.current` -
adding a second request/handler pair for the same property, dispatched through the same authority check, into
the same clamp, would have been a second copy of `on_apply_damage`'s shape wearing a different name, not a
genuinely different mechanism (`healing_test.cpp` proves the reuse directly: every case dispatches
`health::ApplyDamage`, never a distinct type).

**Armor mitigates incoming damage, never incoming healing.** This is the one place the two directions'
behavior actually diverges, and it's deliberate: `on_apply_damage` only reads the target's composed `Armor`
value and computes a mitigation when `cmd.amount > 0` (see `health.cpp`) - a heal is not "negative damage" that
Armor happens to blunt from the other side, it's a different kind of event Armor has no opinion on at all.
`healing_test.cpp`'s `HealIsNotMitigatedByArmor` is the test that actually proves this: it gives the target a
substantial Armor contribution (mirroring `combat_scenario_test.cpp`'s own armor setup) and confirms a negative
`amount` still lands in full, unmitigated - the behavior this redesign exists to prove, not merely restate.

Both directions still clamp identically to `[0, Health.maximum]` (`std::clamp(health.current -
effective_change, 0, health.maximum)`, unchanged from before this) - a heal capping at `maximum` is the same
clamp expression that already capped damage at `0`, just hit from the other side, and both still publish the
same `HealthChanged` event on success.

## Seeking a point

`pathing` sits on top of `movement`, proving out capability-to-capability internal dispatch (spec §6,
Terminology: Request vs. Internal Dispatch) for the first time in this demo: `AdvancePathing`'s handler
(`pathing::on_advance_pathing`) never mutates `movement::Position` itself, and never re-implements `Move`'s own
validation or arithmetic - it computes a normalized direction from the entity's current `movement::Position`
toward its `PathTarget`, then calls `movement::on_move` directly, the same "call the owning capability's own
function, never reach into its state" discipline `equipment::on_equip_armor` already established for
`armor::add_contribution`. A missing `movement::MovementSpeed` therefore surfaces as `movement::on_move`'s own
`"target has no MovementSpeed"` rejection, unchanged - `pathing_test.cpp`'s
`AdvancePathingPropagatesMovementsOwnRejectionWithoutMovementSpeed` proves this is a real dispatch, not a
parallel reimplementation.

**Single target, not a queued path.** `atlas-cgen`'s manifest type system is scalar-only right now
(`int8`-`uint64`, `float`, `double`, `bool`, `EntityRef`, `ResourceId` - see `tools/atlas-cgen/README.md`'s "Type
mapping") - there is no list/array field type yet, so a multi-waypoint queued path is out of scope for this
round. `SetPathTarget` overwrites `PathTarget`'s single `(target_x, target_y)` outright; a caller wanting
waypoints today issues one `SetPathTarget` per leg itself.

**`has_target == false` is treated as an ordinary idle state, not a rejection.** Deciding this meant picking
between two existing precedents in this codebase: `health.cpp`'s treatment of a missing `armor::Armor` (legitimate,
no-mitigation, not an error - not every damageable entity composes armor) versus `movement.cpp`'s treatment of a
missing `Position`/`MovementSpeed` (a hard reject - an entity issuing `Move` is expected to have both, so a
missing one is a setup mistake). A `PathTarget` with `has_target == false` resembles the Armor case, not the
Position/MovementSpeed one: `PathTarget` legitimately toggles `has_target` back to false both before any target
is ever set and again on arrival, so "nothing to seek right now" is a real, expected steady state a caller can
poll every tick - the same way a server might call `AdvancePathing` unconditionally for every pathing-capable
entity each tick, whether or not it currently has anywhere to go. A `PathTarget` property that was never seeded
at all for the entity is still a hard reject (`"target has no PathTarget"`), matching Position/MovementSpeed's
setup-mistake reasoning exactly - the property missing entirely is a different situation from the property
existing with `has_target == false`.

**Arrival epsilon.** `on_advance_pathing` clears `has_target` (and publishes `PathTargetReached`) once within a
fixed `0.01` distance of the target, checked *before* that tick's movement rather than after - so the tick that
lands exactly on the target does not itself clear `has_target`; that happens on the *next* `AdvancePathing` call
(`pathing_test.cpp`'s `AdvancePathingMovesTowardTargetAcrossSeveralTicks` /
`AdvancePathingArrivesOnceWithinEpsilon` prove this two-call sequence explicitly). Exact overshoot/snapping
precision - stopping precisely on the target rather than wherever the last full-speed step happens to land, or
detecting an overshoot that jumped clean past the target in a single large `delta_ticks` step - is a deliberate
scope cut, not an oversight: this demo's own worked scenarios never take a single step large enough to blow past
the epsilon band, and building real overshoot detection/clamping is a separate, sizable increment on its own.

## What this deliberately does *not* build (and why)

Building all of §7/§8/§20/§6 in full, in one round, would be a separate epic on its own — each of the following
is a real, sizable feature this demo intentionally stays inside a smaller boundary around, for now:

- **No manifest-driven capability composition.** A real Atlas host is assembled by tooling from capability
  manifests, resolving `depends_on` into a graph (§7, §8). That generator doesn't exist yet. `SimulatedHost`
  (in `tests/combat_scenario_test.cpp`) hand-composes capabilities into a host directly in C++ — the same scope
  boundary `atlas-runtime`'s own `Host` already draws around itself ("the hand-composed runtime substrate such a
  manifest-driven host would eventually sit on top of").
- **Only two composition strategies (Additive, Multiplicative).** Spec §20 names seven; only these two have a
  working evaluator (`atlas::runtime::resolve_additive`, `resolve_multiplicative`). The other five (Override,
  Priority Override, Set Union, Ordered Composition, Weighted Composition) each have genuinely different
  resolution semantics that don't generalize from either of these, and are each their own future increment.
- **No standing, general-purpose contribution registry.** `atlas::armor::ContributionRegistry` and
  `atlas::movement::ContributionRegistry` are each capability's own private per-entity bookkeeping (owned
  per-host, passed explicitly to `add_contribution`/`add_speed_contribution` — never a namespace-scope global,
  which would silently collide between independently-created `Host` instances allocating entity indices from 0
  each). Now that a second, differently-shaped registry exists (`movement`'s tracks a declared base per entity;
  `armor`'s doesn't need to), a generic version in `atlas-runtime` itself is a more concrete future increment
  than it was with only one example — but building that generalization from exactly two data points would still
  be premature.
- **No client-side prediction or reconciliation** (spec §6). Client A, in this demo, simply waits for the
  server's replicated result — it never locally guesses `Health`'s outcome before the server confirms it.
- **No multi-waypoint queued pathing.** `pathing`'s `PathTarget` holds exactly one `(target_x, target_y)`, not a
  list — `atlas-cgen`'s manifest type system has no list/array field type yet (see "Seeking a point" above), so a
  real waypoint queue is a future increment gated on that generator capability existing first, not a decision
  this round makes on gameplay grounds.
- **No exact arrival/overshoot precision for pathing.** `AdvancePathing` checks its arrival epsilon before that
  tick's movement, not after — a single `delta_ticks` step large enough to blow straight past the target without
  ever landing inside the epsilon band is not detected or corrected (see "Seeking a point" above).
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
