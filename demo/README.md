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
├── main.cpp                 # demo-host: the runnable in-process host executable (issue #70)
├── host_loop.hpp/.cpp        # demo::run_ticks - the real tick-loop mechanism demo-host drives
├── demo_host.host.yaml       # host manifest demo-host composes via (mirrors simulated_host.host.yaml)
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
│   ├── pathing/
│   │   ├── pathing.capability.yaml
│   │   ├── pathing.hpp
│   │   └── pathing.cpp
│   ├── aura/
│   │   ├── aura.capability.yaml
│   │   ├── aura.hpp
│   │   └── aura.cpp
│   ├── line_of_sight/
│   │   ├── line_of_sight.capability.yaml
│   │   ├── line_of_sight.hpp
│   │   └── line_of_sight.cpp
│   ├── attack_resolution/
│   │   ├── attack_resolution.capability.yaml
│   │   ├── attack_resolution.hpp
│   │   └── attack_resolution.cpp
│   ├── auto_attack/
│   │   ├── auto_attack.capability.yaml
│   │   ├── auto_attack.hpp
│   │   └── auto_attack.cpp
│   ├── cast_time_attack/
│   │   ├── cast_time_attack.capability.yaml
│   │   ├── cast_time_attack.hpp
│   │   └── cast_time_attack.cpp
│   ├── interruption/
│   │   ├── interruption.capability.yaml
│   │   └── interruption.hpp        # no .cpp - no hand-written logic, see its own section
│   ├── haste/
│   │   ├── haste.capability.yaml
│   │   ├── haste.hpp
│   │   └── haste.cpp
│   ├── damage_over_time/
│   │   ├── damage_over_time.capability.yaml
│   │   ├── damage_over_time.hpp
│   │   └── damage_over_time.cpp
│   ├── rigid_body/                    # atlas-physics DAG integration, issue #188 - see "Physics" section below
│   │   ├── rigid_body.capability.yaml
│   │   ├── rigid_body.hpp
│   │   └── rigid_body.cpp
│   └── physics_observer/              # minimal downstream consumer of rigid_body's BodyState, issue #188
│       ├── physics_observer.capability.yaml
│       ├── physics_observer.hpp
│       └── physics_observer.cpp
└── tests/
    ├── simulated_host.hpp        # shared test scaffolding (SimulatedHost) - not a capability
    ├── simulated_host.host.yaml  # host manifest SimulatedHost composes via (see atlas-cgen's README)
    ├── physics_host.host.yaml   # deliberately minimal host manifest rigid_body_test.cpp composes via (issue #188)
    ├── combat_scenario_test.cpp
    ├── equipment_test.cpp
    ├── health_test.cpp
    ├── armor_test.cpp
    ├── movement_test.cpp
    ├── healing_test.cpp
    ├── pathing_test.cpp
    ├── aura_test.cpp
    ├── line_of_sight_test.cpp
    ├── attack_resolution_test.cpp
    ├── auto_attack_test.cpp
    ├── cast_time_attack_test.cpp
    ├── haste_test.cpp
    ├── damage_over_time_test.cpp
    ├── rigid_body_test.cpp      # issue #188 - the real DAG-ordering proof, see "Physics" section below
    └── fireball_test.cpp        # no matching modules/fireball/ - see "Fireball" section below
```

Each module's manifest is generated into a real, compiling C++ contract via `atlas-cgen` (`cmake/GenerateCapabilityContract.cmake`,
the same shared helper `tests/atlas-cgen`/`tests/atlas-contracts` use), and its `.hpp`/`.cpp` is the hand-written
manual implementation a capability author writes against that contract (spec §14).

## Runnable host (issue #70)

Before this issue, `demo/` only proved capabilities through test harnesses (`SimulatedHost` in `demo/tests/`) —
there was no actual running process. `demo-host` (`main.cpp`) is a real `main()` composing every `demo/modules`
capability into an actual host process, built the same way `SimulatedHost` is: `demo_host.host.yaml` mirrors
`demo/tests/simulated_host.host.yaml`'s composed-capability list exactly, generated into a `DemoRuntimeHost`
struct + `register_property_stores()` via the same `atlas_generate_host_composition()`/`atlas-cgen --host`
mechanism (spec §14). A real `atlas::runtime::Host` + `atlas::Context` is constructed against it — not a
parallel, hand-rolled substitute.

**`host_loop.hpp`/`host_loop.cpp`** (`demo::run_ticks`) is the actual tick-driving mechanism, and this
codebase's first exerciser of `Host::run_tick()`/`Context::end_tick()` (via `atlas::advance_tick`) end to end
for demo capabilities — every `demo/tests/*.cpp` scenario elsewhere drives time via a `delta_ticks` field on an
individual request instead, never a real scheduled/ticked loop. `run_ticks` is deliberately mechanism-only
(advance the tick boundary, invoke an optional per-tick callback) so it stays deterministic and unit-testable
without any wall-clock dependency (`demo/tests/host_loop_test.cpp`) — `main.cpp`'s own real-time pacing (a
fixed 60 Hz sleep loop, `atlas::core::Time::ticks_per_second`), heartbeat logging, and `SIGINT`/`SIGTERM`
handling all live in the callback and the surrounding loop in `main.cpp` instead, which is why that file itself
isn't unit tested (the same `tools/*/src/main.cpp` convention `cmake/CodeCoverage.cmake` already excludes from
the coverage gate — CLI entry points are integration-level).

`demo-host [--ticks N]` runs at real time indefinitely until interrupted by default, or exactly `N` ticks as
fast as possible (no pacing) in a bounded smoke-test mode.

**Deliberately scoped to composition only, no new gameplay semantics** (this issue's own scope, matching
`demo/`'s general scope boundary below): no entities are created and no requests are dispatched anywhere in
`main.cpp` today. With no real input/render/audio wired in yet, there is nothing yet driving player intent or
observing composed state — an empty, ticking host is the honestly-scoped thing to build. The companion issue
wiring `atlas-input`/`atlas-render`/`atlas-audio` into this same host (using `atlas-windowing`'s shared SDL3
window, issue #174) is what gives this loop something to actually do each tick.

## Orb demo: separate server/client/editor processes (issue #277)

Part of the live-orb epic (#276): a second, deliberately minimal host manifest (`orb_host.host.yaml`, composing
only `movement` — no new gameplay capability, per the epic's own scope) drives three genuinely separate
executables instead of `demo-host`'s single process: `server-host` (authoritative), `client-host` (observer),
`editor-host` (issues `movement::Move` requests). `demo/orb_host.hpp`/`orb_host.cpp` hold the shared
`OrbApp`/`spawn_orb`/`run_paced` scaffolding all three link against via the lean `demo-orb-host` CMake target —
deliberately excluding `atlas::render`/`atlas::input`/`atlas::audio`/`atlas::windowing` (§13: a headless server
host must never gain a dependency on any of them; client/editor stay on the same lean target since neither
wires a real backend yet either).

**No transport between them yet** (issue #278's own scope) — each process spawns and ticks its own local orb
today, so this round's real deliverable is proving three separate, concurrently-running OS processes exist and
build correctly, not that replication works. `editor-host` is constructed with local authority as an explicit,
commented stand-in (`on_move` correctly rejects a non-authoritative request per spec §6 — proven by trying the
honest way first) until #278 lets it dispatch to the real, separate `server-host` process instead.

`demo/scripts/run_orb_demo.sh [build-dir] [--ticks N]` launches all three as real background processes and
tears them down together on exit/Ctrl+C.

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
- **Property replication**: the server's `Health` value is genuinely serialized and decoded on each client — not
  copied by reference, not shared in memory. As of issue #18, this goes over `atlas-replication`'s generic,
  reflection-driven property codec (`atlas::replication::write_property_id`/`write_property_fields`,
  `read_property_id`/`read_property_fields`) rather than a hand-written `health::write_health`/`read_health`
  call: the wire tuple is `(PropertyId::from_name("Health"), current, maximum)`, encoded field-by-field via
  `atlas::reflection::for_each_field` instead of a property-specific encoder `health` itself would otherwise
  have to write. Both clients independently decode the identical bytes to the identical value. See
  `atlas-replication`'s own README ("PropertyId, and a generic reflection-driven property field codec") for the
  full mechanism, and "What this deliberately does *not* build" below for what this doesn't yet cover
  (composed-property replication strategies, struct-typed fields).

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

**`movement::refresh_speed_with_transient_contributions` is why `movement` needed one more function beyond
`add_speed_contribution`.** Not every consumer of Multiplicative composition can be a `Permanent` contribution
added once by a discrete request (equip) - see "Range-based auras" below for the consumer that needed this and
why.

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

## Range-based auras

`aura` proves out the `WhileCondition` lifetime (spec §20): `ActivateAura` seeds a source entity's declared
`range`/`multiplier`, and `RefreshAuraEffect` - the per-tick re-evaluation this mechanism needs - computes the
straight-line distance between source's and target's `movement::Position` and applies `multiplier` to target's
`MovementSpeed` only while target is within `range`.

**Why this couldn't reuse `add_speed_contribution`/a `remove_speed_contribution`.** An earlier version of this
work built exactly that - an imperative add-when-in-range, remove-when-out-of-range pair, mirroring
`armor::add_contribution`'s shape. It didn't survive contact with what `WhileCondition` actually means: nothing
*fires* an event when a target walks out of range - "still in range" is a fact about the current tick, not an
occurrence, so there's no discrete moment to call a removal function at (see `atlas-runtime/README.md`'s Scoping
decisions for that reverted design). The correct model, and the one this capability actually uses:
`on_refresh_aura_effect` builds a single ephemeral `Contribution<float>` tagged `WhileCondition` when target is
in range (an empty span otherwise), and calls `movement::refresh_speed_with_transient_contributions` - a new
function that folds the ephemeral contribution together with target's stored `Permanent` ones for *this one
resolution only*, writes the result, and never persists the ephemeral part anywhere.
`RefreshAuraEffectStopsApplyingOnceTheTargetLeavesRange` is the test that actually proves this: the exact same
call, `RefreshAuraEffect{source, target}`, is dispatched twice - once with target in range, once after target
has moved out - and the effect simply isn't there the second time, with no "remove" step in between.

**Range 0 is not a special case.** `on_refresh_aura_effect` computes `distance(source, target) <= range`
unconditionally; when `source == target` (a self-buff) distance is always `0.0`, which is `<= 0.0`. This is
exactly the "self simply sets range to zero" unification a zone-effect aura and a self-only buff share - no
separate code path (`RefreshAuraEffectAppliesToSelfWhenRangeIsZero` proves it).

**Deliberately out of scope this round:**

- **No target filter.** Every entity within range is affected - there's no notion of ally/enemy/faction
  anywhere in this demo yet to filter by. Adding one is a natural next increment once a scenario actually
  needs to distinguish valid targets, not before.
- **Only `MovementSpeed`.** `on_refresh_aura_effect` is written directly against
  `movement::refresh_speed_with_transient_contributions` - it is not generic over "which property this aura
  affects." A second aura targeting a different composed property (e.g. a `Health` regen zone) would need its
  own analogous handler and its own `refresh_*_with_transient_contributions`-shaped function on whichever
  capability owns that property, following this one as the template - not a shared, type-erased "affects any
  property" mechanism, which Atlas's compile-time composition model (spec §5) doesn't support without giving up
  the very thing that makes it fast and mechanism-checkable. Generalizing the *shape* (range check + ephemeral
  contribution + fold-and-resolve) into something reusable is a reasonable next step once a second concrete
  target property actually needs it - not before.
- **No tick scheduler driving `RefreshAuraEffect` automatically.** Every test in `aura_test.cpp` dispatches it
  explicitly, simulating what a real per-tick scheduler job would do - this demo doesn't build that job itself
  (see `atlas-scheduler`'s own scope for why a generic tick-driven job system is a separate, already-existing
  piece this demo simply doesn't wire up yet). `rigid_body`'s own `step()` job (see "Physics" below) is the one
  deliberate exception - `PhysicsBackend::step()` must run exactly once per tick regardless of what else
  happens that tick, a shape the `Advance*`-request pattern above doesn't fit, so it uses
  `atlas::runtime::Host::schedule` directly instead.

## Line of sight

`line_of_sight` proves out a query-shaped capability - a hand-written function reading properties and computing
a fact, rather than a dispatched request mutating state. `blocks_line_of_sight(ctx, LineOfSightQuery{obstacle,
source, target})` answers whether a circular `Obstacle` (`center_x`, `center_y`, `radius`) sits between
`source`'s and `target`'s `movement::Position`, using the standard point-to-segment distance construction:
project `Obstacle`'s center onto the segment, clamp that projection to the segment's own endpoints (`[0, 1]`),
then compare the closest point's distance to `radius`. `LineOfSightQuery` is a named parameter bundle, not three
adjacent `EntityRef` arguments - a real `bugprone-easily-swappable-parameters` finding from the full clang-tidy
sweep, fixed structurally (designated-initializer construction) rather than suppressed.

**Why this is a function, not a request.** Every other capability's operation so far has either mutated
authoritative state through a dispatched, authority-checked request (`ApplyDamage`, `Move`, `EquipArmor`, ...)
or read another capability's contract directly (`health` reading `armor::Armor` via `ctx.get`). A line-of-sight
check is the second shape, not the first: it mutates nothing, so there's nothing for spec §6's authority
validation to apply to, the same reason reading `Armor`'s composed value never went through a request either.
A future spell-casting capability would call `blocks_line_of_sight` directly from inside its own
authority-checked request handler - the same "call the owning capability's own function, never reach into its
state" discipline `pathing`/`equipment` already established for `movement::on_move`/`armor::add_contribution` -
not reimplement the geometry itself.

**One named obstacle per call, not a scan of every obstacle in the world.** `atlas::runtime::PropertyStore<T>`
has no iteration in its public interface (`get`/`set` only), so there's no way to ask "every entity with an
`Obstacle` property" - a caller checking line of sight against several candidate obstacles issues one
`blocks_line_of_sight` call per obstacle instead.

**Clamping, not an infinite line.** `ClosestPointClampsToSourceWhenObstacleProjectsBeforeIt` and
`ClosestPointClampsToTargetWhenObstacleProjectsPastIt` both place the obstacle well outside the segment itself,
along the *line* the segment sits on - proving the projection is clamped to `[0, 1]` and not left unclamped,
which would incorrectly treat obstacles anywhere along the infinite extension of the segment as blocking.

**A degenerate (zero-length) segment is a point check, not a divide-by-zero.** `source == target`'s `Position`
makes the projection's denominator `0` - guarded by skipping the projection entirely (`t = 0`) rather than
dividing, degenerating to comparing `Obstacle`'s center directly against that single point.

## Attack resolution

`attack_resolution` hosts `resolve_targeted_attack` - the range, line-of-sight, and damage-application sequence
factored out of `auto_attack::on_try_auto_attack` once it became clear a second targeted-attack shape (a future
instant/"Sinister Strike"-style request, a future spell cast) would need the exact same sequence, not a
different one. Rather than each new attack or spell reimplementing "is the target in range, is line of sight
clear, apply the damage," they all call this one function - the same "share the mechanism, don't duplicate it"
precedent `pathing::on_advance_pathing` already set for `movement::on_move`.

**An intentionally empty generated contract.** `attack_resolution.capability.yaml` declares no
properties/requests/events of its own - a name-only manifest (`capability: name: attack_resolution` plus
`depends_on`) is a legal, tooling-supported shape (confirmed directly against `atlas-cgen`'s manifest parser and
its own test suite, not assumed). This capability's only job is hosting a function that reads *other*
capabilities' already-existing contracts (`movement::Position`, `line_of_sight::Obstacle` via
`blocks_line_of_sight`, `health::Health` via `on_apply_damage`) - it has no state of its own to declare.

**`TargetedAttackQuery` takes plain values, not a property read from inside the function.** `min_range`,
`max_range`, and `damage` are parameters, not something `resolve_targeted_attack` fetches from a specific
property itself: today `auto_attack::on_try_auto_attack` sources them from `WeaponAttack`, but a future
spell-cast request would source its own range/damage from its own property instead. `resolve_targeted_attack`
only cares about the resolved values, never where they came from - the same separation of concerns
`LineOfSightQuery`'s named parameter bundle already establishes.

**`TargetedAttackOutcome` distinguishes "rejected," "valid no-op," and "landed."** `result` is exactly what the
caller's own request handler should return - unchanged, never reconstructed - whether that's this function's
own `reject` (missing `movement::Position` on either side) or `health::on_apply_damage`'s own propagated result
once an attack is actually attempted. `landed` is the piece a plain `RequestResult` can't express on its own:
`result.accepted == true` is true both for "out of range, nothing happened" and "damage was actually applied" -
`landed` is what lets a caller like `auto_attack` decide whether to reset its own cooldown and consume its own
`pending_bonus_damage`, without `attack_resolution` needing to know either of those concepts exist.

## Auto-attack

`auto_attack` proves out the cyclic melee/ranged auto-attack: `TryAutoAttack`, driven explicitly each call
(`delta_ticks` simulation ticks elapsed since the last call - the same "caller simulates the tick" pattern
`aura`/`pathing` already establish, not a scheduler job this demo builds itself), ticks `WeaponAttack`'s own
cooldown down and, once off cooldown, delegates range, line-of-sight, and landing entirely to
`attack_resolution::resolve_targeted_attack` (target within `[min_range, max_range]` of attacker's current
`movement::Position`, and, when an `obstacle` is given, `line_of_sight::blocks_line_of_sight` doesn't block the
shot).

**Range is `int32`, not `float`.** `min_range`/`max_range` (here, on `attack_resolution::TargetedAttackQuery`,
and on `cast_time_attack::CastTimeAttack`/`BeginCast`) are whole units - a weapon's or spell's range is
authored content, not a computed quantity, and real content is always round numbers ("5 yards"), never
"5.37". `movement::Position` and the distance computed from it stay `float` (a continuous world-space
quantity); only the configured threshold being compared against is an integer, cast to `float` once at the
comparison site (`resolve_targeted_attack`, `aura::on_refresh_aura_effect`) rather than propagated as a
narrower type through the whole calculation.

**One range pair, not a melee/ranged type.** `WeaponAttack` has no "is this melee or ranged" field at all -
`min_range`/`max_range` are the entire model. A melee weapon is just a small `max_range` with `min_range == 0`;
a ranged weapon with `min_range == 0` can be swung at melee distance too (no dead zone), and one with
`min_range > 0` can't - "melee" and "ranged" are tooltip/flavor text and specific-ability requirements a real
game would layer on top, not a distinction the mechanism itself needs to know (spec §2, Mechanism Over Meaning).
There's no "auto-attack switches weapons based on range" logic to build either: a single `WeaponAttack`'s
min/max already covers its whole valid range continuum in one check, so nothing needs to switch.

**Line of sight is required for every swing, not just ranged ones** - `attack_resolution` checks `obstacle`
whenever it isn't `EntityRef{}` (`is_null()`), regardless of how short `max_range` is. `obstacle` being the null
sentinel isn't a "skip the LOS requirement" escape hatch - it means the caller has already determined there's
nothing in the scene worth checking against (the same "one named obstacle per call" scope `line_of_sight` itself
already documents), not that melee is somehow exempt from the requirement.

**Two ability shapes, only one of which needed new mechanism.** The original design conversation named two
kinds of ability: one that *enhances* a single auto-attack (a Heroic Strike shape) and one that's a *separate,
instant* attack (a Sinister Strike shape).

- `QueueAttackBonus` is the first shape: it accumulates onto `WeaponAttack::pending_bonus_damage` (`+=`, so more
  than one queued bonus can stack before the next swing lands) and touches nothing else - no cooldown change, no
  damage dealt, no target even named. It doesn't attack; it primes whichever swing `TryAutoAttack` lands next
  (`TryAutoAttackConsumesPendingBonusDamageOnLanding` proves the bonus is added to that swing's damage and reset
  to `0`, not carried forward again).
- The second shape needed **no new mechanism at all**: it's just another request dispatching
  `health::on_apply_damage` directly, entirely independent of `WeaponAttack`.
  `InstantAttackBypassesTheAutoAttackCooldownEntirely` proves this rather than merely asserting it -
  `WeaponAttack::cooldown_remaining_ticks` is bit-for-bit unchanged by a directly-dispatched `ApplyDamage`.

**Landing is `attack_resolution`'s own verdict, checked before consuming anything.** `on_try_auto_attack` never
checks `movement::Position`, `line_of_sight::Obstacle`, or `health::Health` itself - it reads
`TargetedAttackOutcome.landed` and, if `false`, returns `outcome.result` (or an unchanged accept as a no-op)
without touching `pending_bonus_damage`/`cooldown_remaining_ticks` at all. A target with no `Health` surfaces
`health::on_apply_damage`'s own rejection reason unchanged, all the way up through `attack_resolution`
(`TryAutoAttackPropagatesHealthsOwnRejectionWithoutHealthOnTarget`). Because `landed` is only ever `true` once
that dispatch actually accepted, a swing that didn't connect never consumes either field - exactly as if it had
never been attempted.

## Cast-time attacks

`cast_time_attack` proves out the second real caller `attack_resolution` was extracted for: a targeted attack
that only resolves after a wind-up, the same mechanism whether the flavor text calls it a spell's cast bar or a
melee ability's wind-up (spec §2, Mechanism Over Meaning - `cast_time_attack` doesn't know or care which). Two
requests: `BeginCast` starts the wind-up (`caster`'s `CastTimeAttack` property records `target`, `obstacle`,
`min_range`/`max_range`, `damage`, `animation`, and the *effective* `cast_time_ticks` - see "Haste and cast
animation" below for where that comes from - with `remaining_ticks` reset to match);
`AdvanceCast` - driven explicitly each call (`delta_ticks`), the same "caller simulates the tick" pattern
`auto_attack`/`aura`/`pathing` already establish - ticks `remaining_ticks` down and, once it reaches `0`,
delegates entirely to `attack_resolution::resolve_targeted_attack` using what `BeginCast` locked in.

**Range and line of sight are checked once, at completion - never at the start.** `on_begin_cast` doesn't touch
`movement::Position`, `line_of_sight::Obstacle`, or `health::Health` at all; a caster can begin casting at a
target that's currently out of range or behind an obstacle, betting on the situation changing before the cast
finishes. `AdvanceCastFizzlesWhenTargetMovedOutOfRangeBeforeCompletion` and
`AdvanceCastFizzlesWhenLineOfSightIsBlockedAtCompletion` both prove this by seeding a target that's valid at
`BeginCast` time and only becomes invalid afterward - if the check ran at the start instead, both tests would
land instead of fizzling.

**`ActionState`, not `remaining_ticks > 0`, is what distinguishes "casting" from "idle."** A `0`-cast-time ability
(an instant spell, a wind-up-free heavy attack) starts with `remaining_ticks` already at `0` right after
`BeginCast` - indistinguishable from "never cast anything" if `remaining_ticks` alone were the signal. This was a
real bug caught by `ZeroCastTimeResolvesOnTheFirstAdvanceCastCall` during TDD, originally fixed with a bare
`is_casting` bool and later generalized into `atlas::runtime::ActionState` (see "Interrupting an in-progress
action" below) once a second, structurally different capability (`auto_attack`) needed the identical shape:
without a real state signal, the first `AdvanceCast` after a `0`-cast-time `BeginCast` would treat the cast as
already-idle and skip resolving it entirely, rather than resolving it immediately as intended.

**A fizzled cast still consumes the wind-up.** Whether `resolve_targeted_attack` reports `landed = true` or
`false` once `remaining_ticks` reaches `0`, `ActionState` moves to `Completed` either way - there is no lingering
"waiting for range to become valid again" state. A fizzled cast must be started over with a fresh `BeginCast`,
exactly like a fizzled spell in most games costs its full cast time for nothing.

**Still no player-initiated cancel.** `BeginCast` rejects outright while the caster's `CastAction` is already
`Started` or `Ongoing`
(`BeginCastRejectedWhileAlreadyCasting`) - there is no request for the caster themselves to voluntarily abort a
cast early. What *does* now exist is cancellation from the outside - movement, or another entity's effect - see
"Interrupting an in-progress action" below.

## Haste and cast animation

Two related additions on top of `cast_time_attack`, both answering the same underlying question: what does a
client actually do while a cast is winding up? Atlas itself never renders anything - it hands over enough
information (a resource identity, a duration) for a client to run its own animation and trusts it to "handle the
rest" (spec §3, Resource; spec §6, replicated state) - but it needed two new pieces to make that information
meaningful: a way for a cast's effective duration to vary, and a way to tell the client which duration to expect.

**`CastSpeed` belongs to `haste`, not `cast_time_attack` - `cast_time_attack` only reads it.** A capability can
read (and, via the direct-store-access pattern below, write) any property registered on the host's `Context`
regardless of which capability's manifest declared it - `aura` already writes `movement::MovementSpeed` without
`movement` knowing `aura` exists. `CastSpeed` (`composition: Multiplicative`, the same strategy
`movement::MovementSpeed` uses) is declared in `haste.capability.yaml`, alongside `HasteSource` - `haste` is the
only thing that produces a casting-speed multiplier, so it owns the property, the registration, and (see below)
the one function that resolves it. `cast_time_attack::on_begin_cast`'s only involvement is a single
`ctx.get<haste::CastSpeed>(cmd.caster)` read - defaulting to `1.0` (no speedup) when the caster has none at all,
exactly like an entity with no `Armor` resolves to no mitigation - dividing `cast_time_ticks` by it once, and
locking the result into both `cast_time_ticks` and `remaining_ticks` for the rest of that cast
(`BeginCastLocksInAShorterDurationWhenCastSpeedIsHasted`). It is deliberately never re-resolved by `AdvanceCast`: a
haste buff activated or refreshed after `BeginCast` has no effect on a cast already in progress, only on casts
begun after it's active. Re-evaluating `CastSpeed` every `AdvanceCast` tick the way a range-based aura
re-evaluates `MovementSpeed` was considered and rejected - a cast's remaining duration speeding up or slowing down
mid-flight, tracking a haste source's own range check flipping tick to tick, is a real thing a client would have
to reconcile its already-playing animation against, and "jittery" was judged worse than "locks in a value that
might be one tick stale." A non-positive `CastSpeed` (an authoring mistake, not a real haste value) is guarded
against rather than trusted in `on_begin_cast`, since dividing by it would otherwise convert an infinite or NaN
`double` into `std::uint64_t` - undefined behavior, not just a wrong number
(`BeginCastTreatsANonPositiveCastSpeedMultiplierAsNoHaste`).

**`haste`: a range-based `CastSpeed` aura, structurally identical to `aura`, and self-contained.** The CastSpeed
analogue of `aura`'s effect on `MovementSpeed`: `ActivateHaste` seeds a source's declared `range`/`multiplier`
(`HasteSource`), and `RefreshHasteEffect` - driven explicitly each call, the same pattern
`aura::on_refresh_aura_effect` establishes - computes the distance to target and writes source's `multiplier`
straight into target's `CastSpeed` when in range, `1.0` (the declared identity) otherwise. Unlike
`movement`/`aura`, there is no `ContributionRegistry` here at all: haste is `CastSpeed`'s only contributor today,
so resolving through `atlas::runtime::resolve_multiplicative` over a stored-plus-transient span would add
ceremony without changing the result - a direct assignment is exactly as correct, and shorter. (If a second,
independent contributor to `CastSpeed` ever shows up, that's the moment to add the registry back - the same
"a second real caller justifies it" reasoning `attack_resolution` was only extracted under, not before.)
`on_refresh_haste_effect` still takes `PropertyStore<CastSpeed>` directly rather than routing through
`Context::get<T>` (which only mutates an entry that already exists, never creates one): a target's first-ever
haste effect is exactly the moment `CastSpeed` starts existing for them, so this is the one function responsible
for both creating and updating it, via `PropertyStore::set`'s own insert-or-assign semantics. `haste` was kept a
separate, small capability rather than generalizing `aura` itself to target an arbitrary property - `aura` stays
hardwired to `MovementSpeed`, matching this demo's existing precedent that each composed property gets its own
small range-effect capability (`movement` + `aura`; `haste` + `CastSpeed`) rather than one capability dispatching
over which property to touch. The result: `cast_time_attack` gained exactly one read and (initially) a `depends_on: [haste]` entry for this whole
mechanism - everything else (the property, its registration, and its resolution) lives entirely inside `haste`'s
own three files. That `depends_on: [haste]` entry was later replaced by `consumes: [CastSpeed]` (issue #16, see
"Property-level `consumes:`" below) - `cast_time_attack` now names the property it needs rather than the
capability that happens to provide it.

**`CastStarted`: the animation resource identity, and the duration a client can actually trust.** A new event,
published from `on_begin_cast` on acceptance (never from `AdvanceCast`): `caster`, `animation` (the `ResourceId`
`BeginCast` was given - authored content, the same way `equipment::EquipArmor::item` is, never a hard-coded path,
spec §3), and `duration_ticks` - the *effective*, already-hasted duration, not the request's original
`cast_time_ticks`. This is the whole point: a client sizing its own animation playback to `duration_ticks` will
have it finish exactly when the cast itself does, whether or not haste was involved
(`BeginCastPublishesCastStartedWithTheAuthoredDurationWhenNoHasteIsActive` proves the unhasted case still
publishes correctly). `HastedCastStillCompletesAfterItsShortenedDuration` is the concrete scenario this whole
mechanism exists for: a caster with `CastSpeed` at `2.0` begins a `10`-tick cast, locks in a `5`-tick effective
duration, and driving `AdvanceCast` for exactly those `5` ticks - not the original `10` - lands the cast
(`CastLanded` publishes, target takes damage) rather than fizzling or leaving it still in progress. The simulated
animation is shorter, but it completes.

## Damage over time, and Fireball: content is data, not a capability

Answers a real question raised during review: to add a new spell, does a game need a new *capability* - a new
manifest, a new generated contract, a new build step - per spell? No. Capabilities model **mechanisms**
(spec §2, Mechanism Over Meaning); a spell is **content** - specific numbers fed into mechanisms that already
exist. Adding Fireball costs zero new capabilities, proven end-to-end by `fireball_test.cpp` (deliberately not
under `modules/` - there is no `fireball.capability.yaml` to put there).

**`damage_over_time`: the one genuinely new, reusable mechanism this needed.** Direct damage
(`cast_time_attack`/`auto_attack` via `attack_resolution`) already existed; recurring damage over a duration
didn't. `ApplyDotEffect` seeds a target's `DotEffect` (`damage_per_tick`, `tick_interval_ticks`,
`remaining_applications`); `AdvanceDotEffect` - driven explicitly each call, the same "caller simulates the
tick" pattern every other capability in this demo establishes - counts `ticks_until_next` down and, once it
reaches `0`, dispatches `health::on_apply_damage` directly (an internal dispatch, not reimplemented, the same
precedent `attack_resolution`'s own callers set) for exactly one application, publishing `DotEffectTicked` and
decrementing `remaining_applications`. `damage_over_time` has no notion of fire, poison, or bleed - it is
exactly as generic as `cast_time_attack` not knowing whether it's a spell or a melee wind-up. Two scope cuts,
both documented the same way `pathing`'s own epsilon-check limitation already is: **no stacking** (a single
`DotEffect` slot per target - a fresh `ApplyDotEffect` discards whatever was left of a previous one and starts
over, `ApplyDotEffectRefreshesAnAlreadyActiveEffect`), and **no multi-application catch-up** (`AdvanceDotEffect`
fires at most one application per call regardless of how large `delta_ticks` is, even if it spans more than one
full `tick_interval_ticks`).

**Fireball itself: zero new types, one subscription.** `fireball_test.cpp`'s `Fireball` test dispatches
`cast_time_attack::BeginCast` with plain authored constants (`damage = 100`, `cast_time_ticks = 60`, a
`min_range`/`max_range`, an `animation` `ResourceId`) - fields `BeginCast` already declares for *every*
cast-time attack, not anything Fireball-specific. The one piece of real code - "landing a Fireball also starts
a burn worth 20% of the direct hit, three times, three seconds apart" - is a single `ctx.subscribe<CastLanded>`
lambda reacting to `cast_time_attack`'s own already-generic landing event, dispatching
`damage_over_time::ApplyDotEffect` with `damage_per_tick = landed.damage / 5`. That's the entire cost of "Fireball
also burns": no manifest, no `PropertyStore`, no build step - a handful of lines, the same size and shape as the
cancellation-wiring lambdas `demo/tests/simulated_host.hpp` already has. `core::Time::ticks_per_second == 60`
makes 3 seconds `180` ticks and 9 seconds (3 applications, 3 seconds apart) exactly `3 x 180` ticks of
`AdvanceDotEffect` - `DirectHitPlusBurnDealsTheFullExpectedTotal` drives the whole pipeline (`BeginCast` →
`AdvanceCast` lands → `CastLanded` → `ApplyDotEffect` → three `AdvanceDotEffect` calls) and asserts the total:
`200 - 100 (direct hit) - 3 x 20 (burn) == 40`.

**What this doesn't solve, and was never trying to.** `CastTimeAttack`/`BeginCast` have no `damage_type` field -
`atlas-cgen`'s manifest type system has no enum field type yet (a real, previously-flagged tooling gap; a
"Fire"/"Ice"/"Physical" tag today would have to be an `int32` with app-level meaning, or a `ResourceId` naming a
damage-type resource, neither as type-safe as a real enum). `damage` is a flat `int32`, not a min-max roll - a
random damage range would need `atlas::core::Random` (already exists, deterministic) wired into a resolve step
that doesn't exist yet. `CastStarted` carries exactly one `animation`/`duration_ticks` pair, not multiple
animation states (wind-up/loop/impact). None of these are attempted here - Fireball's own numbers were chosen
specifically to not need them, and each is a real, separate future increment, not a decision this round makes on
tooling-scope grounds.

**A different, still-valid pattern this doesn't need: a thin specialization capability.** If a *family* of
spells needed genuinely reusable behavior beyond fixed values (not "Fireball's damage type is Fire" but
"every fire spell also ignites nearby flammable terrain"), a small capability wrapping the shared mechanism -
structurally identical to how `haste` wraps `cast_time_attack`'s `CastSpeed` consumption - would be the right
tool. Fireball's burn didn't need one: `on_land, apply a scaled DoT` is a fixed reaction, not a mechanism only a
capability boundary could provide, so the subscription-lambda glue above is deliberately all it costs.

## Beyond Fireball: entity-per-instance effects and declarative content (design proposal, #144)

Fireball proves "a spell is content, not a capability" — but its mechanics still cost a hand-written
`ctx.subscribe<CastLanded>` lambda per spell, and it inherits `damage_over_time`'s documented "no stacking"
limit (see above): a single `DotEffect` slot per target means two simultaneous DoTs from two different sources
can't coexist. A design conversation landed on two further steps, tracked as a proposal in #144, not built here.

**Each DoT (and any other multi-instance effect) becomes its own entity, not a bigger `DotEffect`.** Rather than
growing `DotEffect` into a fixed-size array of slots — capped, with hand-rolled slot management, the same
category of ad hoc bookkeeping `armor::ContributionRegistry`/`auto_attack::ActionRegistry` already are — each
DoT application would be its own entity (`target: EntityRef` plus the existing `DotEffect` fields), ticking
independently through the ordinary property-graph mechanism. Applying a DoT creates an entity; expiry destroys
it — both already-established, deferred, tick-boundary-controlled operations, not new machinery. "Dozens of DoTs
on one target" is then just dozens of ordinary entities referencing the same target, unbounded. The same pattern
covers buffs, HoTs, and channeled effects — and presentation, not just gameplay: **a visible aura is just
another entity anchored to its target** (a `Position` following the target, a resource reference to make it
visible), not a separate "visual effect" concept. A DoT-tick reading its *target's* `Health` (not its own) is
the same cross-entity "gather" need `line_of_sight` already has reading someone else's `Position` — one more
concrete case for it, not a new one. Dispel/cancellation of a duration effect reuses
`atlas::runtime::Cancellable<T>`/`request_cancel`/`advance_action` as-is (see "Interrupting an in-progress
action" below) — a dispel is just another request calling `request_cancel` against the DoT-entity.

**Content becomes a declared sequence of existing generic requests, not per-spell subscription code.** A
`venom_strike`-style attack-plus-poison would be authored as data (a resource, spec §3) naming which already-
existing requests to dispatch (`BeginCast`, then `ApplyDotEffect` if `BeginCast` landed), with what parameter
values — zero new capabilities, zero new hand-written glue, versus Fireball's one lambda today. The harder part
this surfaces: a spell's numbers need to be **moddable** by talents/gear a capability author never anticipated —
which turns out to be nothing more than §20's Triggered Composition applied to a request's own field, not a new
mechanism: the spell's authored number is one contribution, resolved once at cast-time against whatever else is
composing into the same generically-named property.

**Deliberately not built here — see #144 for the full design and what it depends on:**

- The entity-per-instance rework of `damage_over_time` itself (still single-slot, still no stacking, as today).
- Any declared-content/resource format or its execution — Fireball's lambda-per-spell pattern is still how this
  demo adds attacks.
- The runtime registry resolving a request's name to its generated type and a dispatchable callable, which this
  depends on and which is the same gap #129/#141 already need solved for graph-node/request substitution.

## Interrupting an in-progress action

A generic cancellation mechanism, not a `cast_time_attack`-only or `auto_attack`-only one - built on
`atlas::runtime::ActionState`/`Cancellable<T>`/`request_cancel`/`advance_action` (see `atlas-runtime`'s own
README section for the platform-level pieces). Two distinct triggers, each a real answer to a real design
question, sharing one small piece of shared vocabulary (`interruption::ActionInterrupted`) but otherwise living
entirely inside whichever capability has cancellable state of its own.

**Cancellation is noticed and applied in the same call, not queued via a subscription.** `movement::PositionChanged`
and `interruption::ActionInterrupted` are triggered properties (spec §20, Triggered composition; issue #47), not
`EventContract`s dispatched through `Context::publish`/`subscribe` the way they used to be. `on_advance_cast`/
`on_try_auto_attack` read them directly with `ctx.get<T>(entity)` - an ordinary consumes-shaped read, absent
(`nullopt`) unless something wrote them since the last call - right before calling
`atlas::runtime::request_cancel`, which only sets a `cancel_requested` flag; the actual transition to
`ActionState::Cancelled` happens moments later in the same call, via `atlas::runtime::advance_action`, which
checks `cancel_requested` *before* any of that function's own per-tick logic. This is what "the runtime handles
cancel first" means as literal control flow: the capability's own scheduled turn both notices the trigger and
applies it, with no separate subscription/callback wiring in between
(`AdvanceCastCancelsARequiresStationaryCastWhenMovementTriggered`/
`TryAutoAttackAppliesTheFullCyclePenaltyWhenMovementTriggeredAndTheWeaponRequiresStandingStill` prove both steps
land together).

**Trigger one: movement, opt-in per attack.** "Some attacks require the caster to stand still; moving cancels
them" is not a blanket rule this mechanism imposes - it's a per-`WeaponAttack`/per-cast
`requires_stationary: bool` flag, set at `BeginCast`/seeded on `WeaponAttack` like any other authored value.
`movement::on_move` writes `PositionChanged` via `ctx.set<T>(cmd.target, ...)` - a same-tick occurrence, keyed by
the entity whose position changed (no `target`/`entity` payload field needed: `PropertyStore<T>` already scopes
storage per-entity via the `ctx.set<T>(entity, ...)` call site itself). `cast_time_attack::on_advance_cast` and
`auto_attack::on_try_auto_attack` read it directly for their own entity (`ctx.get<movement::PositionChanged>(caster)`/
`(attacker)`), checking `requires_stationary` before calling `request_cancel`, so a weapon or cast that never
opted in is left completely untouched by movement
(`AdvanceCastTicksNormallyWhenMovementTriggeredButTheCastDoesNotRequireStandingStill`/
`TryAutoAttackTicksNormallyWhenMovementTriggeredButTheWeaponDoesNotRequireStandingStill`). Because the triggered
property is per-entity, an unrelated entity's movement is automatically invisible to a different entity's own
read (`AdvanceCastIsUnaffectedByAnUnrelatedEntitysMovement`/`TryAutoAttackIsUnaffectedByAnUnrelatedEntitysMovement`)
- there's no separate "is this about me" filter to write, the storage key already is that filter.

**Trigger two: `interruption::ActionInterrupted`, unconditional.** The actual generic piece: a triggered marker
property with zero fields (spec §2, Mechanism Over Meaning, the same reasoning `attack_resolution`'s empty
contract already documents) that any capability deciding an entity's current action should stop can write via
`ctx.set<interruption::ActionInterrupted>(entity, {})`. `interruption` itself has no requests and - unlike every
other capability so far - no hand-written `.cpp` at all: it exists purely to be included and read. Both
`cast_time_attack::on_advance_cast` and `auto_attack::on_try_auto_attack` read it unconditionally, ignoring
`requires_stationary` entirely - a stun should interrupt a cast or a swing-in-progress regardless of whether
that specific attack cared about movement.

**`ActionState` lives in a companion registry, not the generated property.** `cast_time_attack::CastAction` and
`auto_attack::WeaponAction` (each a plain `{action_state, cancel_requested}` pair, `Cancellable`-checked at
compile time) sit in their own `std::unordered_map<EntityRef, ...>` alongside `PropertyStore<CastTimeAttack>`/
`PropertyStore<WeaponAttack>` - the same "capability's own private per-entity bookkeeping" shape
`armor::ContributionRegistry`/`movement::ContributionRegistry` already establish. Not a manifest field: atlas-cgen's
type system has no enum field type yet (see `atlas-runtime`'s README for why that's a deliberate, not-yet-built
piece of tooling rather than an oversight).

**Two different capabilities, two different meanings of "cancel," and two different defaults.**
`request_cancel`/`advance_action` are the same for both; what happens inside `on_cancel` is each capability's
own decision, exactly as spec §20's Design Rule requires (never reach into another capability's state).
`cast_time_attack` treats a fresh `CastAction` as `Completed` (idle - matching how `BeginCast` is a genuine
opt-in step no entity is in the middle of until it's called) and cancels to that same clean idle state
(`remaining_ticks = 0`) - a caster interrupted mid-cast can begin a fresh `BeginCast` immediately, no lingering
penalty beyond the wind-up already spent. `auto_attack` treats a fresh `WeaponAction` as `Started` instead -
there is no separate opt-in step the way `BeginCast` is, so `TryAutoAttack` must always be able to proceed on an
entity's very first call, and a terminal default would make `advance_action` skip it outright. Its own
`on_cancel` resets `cooldown_remaining_ticks` to `attack_speed_ticks` (a full-cycle penalty, not zero) - but only
when the weapon was actually mid-cycle (`> 0`); already-ready is left alone in both triggers, since there's
nothing in-progress yet to interrupt
(`TryAutoAttackDoesNotPenalizeAnAlreadyReadyWeaponEvenWithAQueuedCancellation`). Unlike `cast_time_attack`,
`auto_attack`'s cycle is perpetual - `on_cancel` moves `action_state` straight back to `Started` rather than
leaving it at `Cancelled`, since there is no "go idle and wait for a fresh BeginCast" state to leave it in.

**No crowd-control capability built here.** Nothing in this demo actually applies a stun or disorient - that's a
real, separate gameplay feature (see the scope-cut bullet below).
`AdvanceCastCancelsWhenActionInterruptedTriggeredRegardlessOfRequiresStationary` and its `auto_attack`
counterpart prove the mechanism directly, the same way `InstantAttackBypassesTheAutoAttackCooldownEntirely`
proved the "instant attack" shape without building a whole ability system: by writing
`interruption::ActionInterrupted` straight from the test, exactly as a future stun would.

## Interactable entities: a generic click-to-act HUD bridge (issues #234, #237)

`door` (`OpenDoor`/`DoorOpened`, unremarkable request-handler shape - reject without authority, reject if
`Door` is missing or already open, flip `open` and publish on success) was this demo's first proof that a real
`atlas-ui`/`atlas-input` UI element can reach authoritative state: `door_hud` originally hand-built one
`atlas::ui::Node` with a hardcoded `Clickable{.intent = "OpenDoor"}` (issue #234). Review on that PR asked the
obvious next question: shouldn't *any* clickable entity - doors, levers, lootable items, NPCs - share one
mechanism instead of one hand-written Node-building function per entity type? Issue #237 is that generalization,
plus a second concrete consumer (`lootable`) to prove it against - not just guessed at from `door` alone.

**`interactable`: one generic property, no behavior.** `Interactable{action: IntentId, designator: ResourceId}`
- `action` names which semantic intent a click on this entity produces (the same `IntentId` vocabulary
`atlas-input`'s `IntentRouter` and `atlas-ui`'s `Clickable` already use); `designator` is what a HUD shows the
player, reusing `atlas::ui::Node::resource`'s existing generic resource-reference slot directly (spec §3,
Resource: never a hardcoded string literal) rather than inventing a second binding mechanism. This needed
`atlas-cgen` to learn a new manifest field type, `IntentId -> atlas::input::IntentId` (mirroring the existing
`SessionId` precedent exactly - `tools/atlas-cgen/src/manifest.cpp`'s `type_map`/`include_map`, proven by a real
compile-check fixture, `tests/fixtures/intent_example.capability.yaml`, the same way `SessionId` was). `door` and
`lootable` each compose `Interactable` (seeding it once per entity is this demo's own test/host-setup concern,
the same way seeding `Door`/`Lootable` themselves already is) - neither `door` nor `lootable` has any
awareness that `interactable`, `atlas-ui`, or HUD composition exist at all.

**`interactable_hud`: one generic Node-builder, replacing door_hud's original per-door one.**
`build_control(ctx, entity)` reads `entity`'s own `Interactable` (nullopt if it has none - not every entity is
clickable) and builds one `Node{resource = designator, clickable = Clickable{intent = action}}` - the exact same
function for a door, a lootable item, or any future interactable entity type. It never knows what "OpenDoor" or
"PickUp" mean (spec §2, Mechanism Over Meaning); the Node it builds is snapshot-read at build time (baked, not
continuously re-resolved, spec §20) since which action/designator an entity has doesn't change tick to tick in
this demo - a future need for that (e.g. a door's designator changing when locked) would be the trigger to
switch `Interactable`'s own fields to bindable ones, not something guessed at now.

**`door_hud`/`lootable_hud`: now just the Intent -> request translator, one per capability.** Node-building
generalized cleanly; translating a produced `Intent` back into a *real, typed* request did not, and structurally
can't - `door::OpenDoor{door: EntityRef}` and `lootable::PickUp{item: EntityRef, collector: EntityRef}` are
different shapes, so each bridge capability keeps its own small `to_open_door_request`/`to_pick_up_request`,
mirroring §19's `health_ui_bridge` worked example's own "only place that knows both sides" role - translation
between two mutually-unaware capabilities is inherently a per-pairing concern, not a mechanism a generic HUD
layer could perform on either capability's behalf.

**`lootable`: the second consumer, proving a genuinely different action shape.** `PickUp{item, collector}` names
two entities (the clicked item and whoever's picking it up) where `OpenDoor` names only one - `Intent` itself
only ever carries the *clicked* entity (`entity`), so `collector` is not derivable from the `Intent` at all;
`lootable_hud::to_pick_up_request` takes it as an explicit second parameter, the same way `Node::try_click`
already takes its own `source` as an explicit, caller-supplied argument rather than inferring it. `PickUp` also
rejects differently than `OpenDoor` - a collected item never becomes collectible again (no toggle), so a second
`PickUp` fails via `Lootable.collected` already being `true`, the "already open" shape reused rather than
reinvented, but firmly a one-way transition where `door`'s is reversible.

**What was deliberately rejected as the second consumer, and why.** An early proposal used `equipment` (hovering
an item to show a tooltip) as the second `Interactable` example. That's a category error, not a valid second
data point: hovering-for-a-tooltip is `Tooltip` (§19's other listed behavior - hover-driven, presentation-only,
produces no `Intent`, no request), a completely different mechanism from `Clickable`'s click-to-act path this
issue generalizes. Building `Tooltip` itself is separate, not-yet-scoped work.

**i18n, and what didn't need building.** `designator: ResourceId` is Atlas's whole i18n mechanism for this kind
of label: a locale's actual string bytes behind a given `ResourceId` are a load-time/host concern - which
locale's blob a host's `ResourceRegistry` loaded - never something `Interactable`/`interactable_hud` decide.
`atlas-rcc`'s `type:` tag is already fully open-ended (`tools/atlas-rcc/README.md`), so a `Text`-typed manifest
entry compiles today with zero tooling changes; per-locale selection is just "which blob path the host
constructs `ResourceRegistry` with," the same mechanism every other resource kind already uses. No new
platform code was needed for this half of #237 at all - only the convention (author one `Text.blob` per locale).

## Physics: proving atlas-physics feeds a real, `depends_on`/`consumes`-ordered dependency graph (issue #188)

`atlas-physics` (`libraries/atlas-physics`) is a runtime library - correctly outside the capability DAG per
spec §5 - but unlike `atlas-render`/`atlas-audio` (presentation-only, explicitly outside the determinism
boundary, spec §4), its simulated body state has to become visible to other capabilities in properly
`depends_on`/`consumes`-ordered fashion, not via an arbitrary direct call from outside the graph. Spec §5's own
`Physics["physics"] --> Entity` diagram (`docs/specification/05-dependency-model.md`) is illustrative; this is
what makes it real. Two new capabilities:

- **`rigid_body`** (`demo/modules/rigid_body/`) - deliberately not named `physics`, which would collide with
  `atlas::physics`'s own C++ namespace. Owns a `PhysicsBackend`-satisfying instance
  (`rigid_body::Backend` - `atlas::physics::NullPhysicsBackend` or `atlas::physics::JoltPhysicsBackend`,
  selected via `ATLAS_DEMO_PHYSICS_BACKEND_JOLT`, defined by `demo/modules/rigid_body/CMakeLists.txt` from the
  same `ATLAS_PHYSICS_BACKEND` CMake option every other consumer of `atlas-physics` already uses - never a
  second, independent choice). `depends_on: [entity, movement]`: `SpawnRigidBody` seeds a new body from the
  target entity's already-composed `movement::Position` (this demo's existing 2D ground-plane coordinate,
  investigated rather than inventing a second source of initial position data) plus a caller-supplied
  `spawn_height` - the vertical (Y) coordinate `movement::Position` has no component for, and the axis Jolt's
  own gravity acts along. Declares a composed `BodyState` property (`position_x/y/z`, `rotation_x/y/z/w` -
  flattened scalars, mirroring `atlas::physics::BodyState`'s own position/rotation shape, the same "no vector
  field type yet" flattening `movement::Position`/`pathing::PathTarget` already use for 2D - `atlas-cgen`'s
  manifest type system has no `Vec3`/`Quaternion` field type).
- **`physics_observer`** (`demo/modules/physics_observer/`) - the minimal downstream consumer: `consumes:
  [BodyState]` (spec §5, Property-Level Ordering - the same idiom `cast_time_attack`'s `consumes: [CastSpeed]`
  already establishes), never a `depends_on: [rigid_body]` entry. Copies `BodyState.position_y` into its own
  `ObservedBodyState.height` each tick it runs.

**The triggering mechanism for body creation: an ordinary authority-checked request (`SpawnRigidBody`), not a
property becoming present or entity composition itself.** This is the idiom every other capability in this
demo already uses for "something needs to start existing for an entity" (`door::OpenDoor`,
`aura::ActivateAura`, `haste::ActivateHaste`) - no new mechanism was invented.

**The triggering mechanism for `step()`: `atlas::runtime::Host::schedule(StageId, Job)` - a real, existing
platform mechanism, investigated rather than invented.** Every *other* per-tick-shaped capability in this demo
(`movement`, `aura`, `pathing`, `auto_attack`, `cast_time_attack`, `damage_over_time`) is driven by a caller
explicitly dispatching an `Advance*`/`Refresh*`-shaped request with its own `delta_ticks` field (see "No tick
scheduler driving `RefreshAuraEffect` automatically" above) - deliberately, since nothing in this demo needed a
"runs every tick regardless of what else happens" job before now. `PhysicsBackend::step()` is different: it
must run exactly once per tick regardless of how many bodies or requests exist that tick, not once per
dispatched request - the `Advance*` pattern doesn't fit. Investigating `atlas-stage`/`atlas-scheduler`/
`atlas-runtime` found this mechanism already exists, just never used by a demo capability until this issue:
`atlas::runtime::Host::schedule(StageId, Job)` registers a `std::function<void()>` against a stage in the
host's own `atlas::stage::StageSequence`, and `Host::run_tick()` - invoked automatically every tick via
`atlas::advance_tick`, the same call `demo/host_loop.cpp`'s `run_ticks` already makes - runs every registered
job exactly once, in the sequence's fixed stage order and, within a stage, in registration order (spec §4).
`demo/host_loop.cpp`'s own doc comment even names this precise gap ("no gameplay logic is scheduled against
any stage here") - `rigid_body::schedule_step_job`/`physics_observer::schedule_observation_job`
(`demo/tests/rigid_body_test.cpp`) are the first demo capabilities to actually call `Host::schedule`, not a
new, ad hoc mechanism built for this issue.

**Registration order is what encodes the dependency graph here, and this is a real, documented gap, not a
silent one.** `atlas-cgen`'s host-composition mode already derives a real dependency order from
`depends_on`/`consumes` edges (`resolve_host_composition`) and uses it to order the generated
`PropertyStore` members - but it doesn't yet generate any per-tick *job*-scheduling code (no capability
manifest declares "runs every tick" today). `demo/tests/rigid_body_test.cpp` registers `rigid_body`'s own step
job before `physics_observer`'s observation job by hand, matching the order the graph would derive
(`physics_observer` consumes `rigid_body`'s `BodyState`), and proves that ordering is load-bearing rather than
assumed: `DownstreamConsumerObservesEachTicksFreshlySteppedBodyStateNeverAStaleOne` asserts
`physics_observer::ObservedBodyState.height` exactly equals `rigid_body::BodyState.position_y` on every tick,
and `ReversedJobRegistrationOrderObservesAStaleHeightOneTickBehind` deliberately registers the two jobs in the
wrong order and confirms the concrete failure mode this would otherwise silently produce: the observer's own
read lags exactly one tick behind the real, freshly-stepped value. Generating job-scheduling code from the
graph the same way `PropertyStore` registration already is would close this gap - a natural, but real, future
increment for `atlas-cgen`, not attempted in this round.

**A `Dynamic` body's fall is proven only under `JoltPhysicsBackend`** (`RigidBody.
DynamicBodyFallsUnderRealGravityWhileStayingInSyncWithTheDownstreamConsumer`,
`ATLAS_DEMO_PHYSICS_BACKEND_JOLT`-gated, mirroring `JoltPhysicsBackend.
DynamicBodyFallsUnderGravityWithinPlausibleRange`'s own plausible-band methodology): `NullPhysicsBackend::step()`
is a genuine no-op, so a body's height never changes tick to tick under it - there is nothing for a wrong
registration order to make stale, which is exactly why the reversed-order test is `JoltPhysicsBackend`-only
too. The default `NULL`-backend build still exercises every other part of the mechanism (request
validation/rejection, `BodyState` seeding from `movement::Position`/`spawn_height`, and the same-tick
consistency invariant, which holds - trivially, since nothing ever changes - under `NullPhysicsBackend` too).

**Out of scope, matching issue #188's own boundary:** collision events surfacing as capability-consumable
events (#187's job); real gameplay semantics (damage, triggers, ragdolls) built on top of this; and real
shape/geometry variety beyond `atlas-physics`'s own default `SphereShape` - this issue is DAG plumbing, not
physics fidelity.

### Camera collision: `atlas-render` and `atlas-physics` meet at the composing layer (issue #182)

`demo/camera_collision.hpp`'s `resolve_camera_collision(backend, pivot, desired_position)` is the actual
motivating use case for the whole physics arc (#176): a follow/orbit camera that doesn't clip through walls.
It sweeps a small sphere from `pivot` toward `desired_position` via `PhysicsBackend::sweep()` (#180) and
clamps the result to the first hit point, or returns `desired_position` unchanged if nothing was in the way.

Deliberately lives in `demo/`, not inside `atlas-render` or `atlas-physics` themselves - discussed directly on
issue #182: no spec/CLAUDE.md rule literally forbids an `atlas-render` → `atlas-physics` dependency (unlike the
`atlas-render`/`atlas-input` sibling-library rule `atlas-windowing` exists to mediate), but the same
avoidable-coupling reasoning still applies, since most `atlas-render` consumers never touch physics and vice
versa. So the helper takes a `PhysicsBackend` and plain position data as ordinary function parameters
(dependency injection at the composing layer) rather than being baked into either library - the same place
`presentation_sync.hpp`'s `sync_transforms` already lives, bridging `movement::Position` into
`render::Transform`.

`demo/tests/camera_collision_test.cpp` (`ATLAS_PHYSICS_BACKEND=JOLT`-gated, mirroring `rigid_body_test.cpp`'s
own Jolt-only tests - `NullPhysicsBackend::sweep()` always returns `std::nullopt`, so there is nothing
real to collide with under the default backend) proves a desired position on the far side of a real static
wall clamps to the wall's near face, not past it - the same real, position-assertion rigor #179/#180 already
established, not a "doesn't throw" check.

**Static-vs-dynamic decision:** this sweeps against whatever `PhysicsBackend::sweep()` itself reports, which -
per #180's own scope - does not distinguish static from dynamic bodies at all (no broadphase-layer/motion-type
filter parameter exists on the contract today). So the camera stops at *any* body's surface, static geometry
or a dynamic prop/NPC alike - the honest consequence of `sweep()`'s current, unfiltered surface, not a
deliberate design choice. Letting the camera pass through a moving dynamic body instead would need a new,
layer-aware sweep overload on `PhysicsBackend` itself - a real, flagged follow-up, not solved here.

**Not wired into `PresentationApp`/`main.cpp`** - this issue adds the standalone mechanism and proves it
against real physics, the same way #181 added `Camera`/`Sdl3FrameBackend::set_camera()` without wiring either
into `PresentationApp`. An actual orbit/follow camera reading real input each tick is a gameplay-level camera
control scheme, explicitly out of scope per §2 (same carve-out #181's own scope already makes) - a real,
undesigned follow-up if `demo/` grows one.

## What this deliberately does *not* build (and why)

Building all of §7/§8/§20/§6 in full, in one round, would be a separate epic on its own — each of the following
is a real, sizable feature this demo intentionally stays inside a smaller boundary around, for now:

- **Manifest-driven capability composition is partial, not complete.** A real Atlas host is assembled by
  tooling from capability manifests, resolving `depends_on` into a graph (§7, §8, §14). `atlas-cgen`'s host
  composition mode (`--host`, see its own README's "Host composition" section) now generates the
  PropertyStore-registration half of this - `demo/tests/simulated_host.hpp`'s `SimulatedHost` composes via a
  real host manifest (`simulated_host.host.yaml`) plus one generated `register_property_stores` call, not 11
  hand-written `ctx.register_property_store(...)` calls. Ordering itself can now also be driven by property flow
  rather than only capability-to-capability naming - a manifest can declare `consumes: [PropertyName]`
  (issue #16) and `atlas-cgen` resolves it against whichever composed capability's own `properties:` block
  provides that name, deriving the ordering edge instead of the consumer naming the provider directly.
  `cast_time_attack`'s manifest is the one worked example so far (`consumes: [CastSpeed]`, replacing a
  `depends_on: [haste]` entry - see "Haste and cast animation" above); every other capability's `depends_on`
  list is untouched, since several of those edges represent direct function calls between capabilities, not
  property flow, and migrating those is a separate, larger increment (see below). Request-dispatch wiring
  (`SimulatedHost`'s own dispatcher registrations in each test) is still entirely hand-written. Event-subscription
  wiring, by contrast, is gone rather than still-manual: `movement::PositionChanged`/
  `interruption::ActionInterrupted` moved from `events:`/`ctx.publish`-`subscribe` to triggered `properties:`
  (spec §20, Triggered composition; issue #47), so there's no subscription decision left for host composition to
  make at all - see "Interrupting an in-progress action" above.
- **`consumes:` doesn't replace `depends_on` for non-property coupling, and most capabilities still use it.**
  `auto_attack` and `cast_time_attack` both call `attack_resolution::resolve_targeted_attack` directly as a
  function call, not through a property read - that's a real `depends_on` edge with no property standing in for
  it, and turning it into one would mean refactoring `attack_resolution` into something that computes and
  publishes a property rather than being called into, which is a genuinely separate, larger change than adding
  the `consumes:` field itself. Two related checks from the original design discussion behind issue #16 -
  aggregating properties with multiple declared contributors, and "no unused providers"/"no dead properties"
  warnings - are deliberately not built either: the first already has a different, existing solution (a
  property's `composition:` strategy, §20) for a related-but-distinct problem ("multiple *contributors* to one
  property's value", not "which single capability is responsible for computing it"), and the second has no
  concrete need driving it yet.
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
- **`pathing` doesn't route around `line_of_sight`'s obstacles.** `AdvancePathing` moves straight toward its
  target regardless of any `Obstacle` sitting between the entity and where it's heading — real obstacle
  avoidance (a navmesh, A*, or similar) is a separate, sizable increment on top of both capabilities existing,
  not a decision this round makes on gameplay grounds. `line_of_sight` exists as a standalone query today,
  callable by anything that needs it (a future spell-casting capability checking "can I hit this target"), not
  yet wired into `pathing`'s own movement decisions.
- **No AI deciding when to issue `TryAutoAttack` or target selection.** Every `auto_attack_test.cpp` case
  dispatches `TryAutoAttack` explicitly with an already-chosen `attacker`/`target` pair - nothing in this demo
  yet decides *who* an entity should be attacking or *when* to keep trying (an NPC's attack/evade decision
  logic is a separate, sizable increment on top of `auto_attack`, `aura`, `healing`, `pathing`, and
  `line_of_sight` all existing, not a decision this round makes on gameplay grounds).
- **`attack_resolution` now has two callers.** It was factored out of `auto_attack::on_try_auto_attack` because a
  second targeted-attack shape was expected reuse, not a hypothetical one - `cast_time_attack::on_advance_cast`
  is that second caller. An instant/"Sinister Strike"-style request (no cooldown, no cast time, just a direct
  `health::on_apply_damage` dispatch per `auto_attack`'s own README section) still doesn't exist in this demo,
  but the two real callers that do exist are enough to confirm the extraction was worth it, not just plausible.
- **No player-initiated cancel.** Movement and `interruption::ActionInterrupted` can both cancel an in-progress
  cast or reset an auto-attack's cooldown (see "Interrupting an in-progress action" above), but there is still no
  request for a caster to voluntarily abort their own cast because they changed their mind - only outside forces
  interrupt in this demo.
- **No crowd-control capability.** `interruption::ActionInterrupted` is real and wired up on both consuming
  sides, but nothing in this demo actually applies a stun, disorient, or silence - `ActionInterruptedCancelsACastRegardlessOfRequiresStationary`
  and its `auto_attack` counterpart prove the mechanism by publishing the event directly from the test, the same
  "prove the shape without building the whole feature" precedent `InstantAttackBypassesTheAutoAttackCooldownEntirely`
  already set. A real crowd-control capability (its own property for duration/type, its own request to apply it)
  is a separate, sizable increment on top of `interruption` existing.
- **A hard "Kick"-style targeted interrupt ability doesn't exist either.** `ActionInterrupted` only carries the
  affected entity - there's no notion of *who* interrupted whom, which a real interrupt ability (with its own
  cooldown, range check, and maybe a lockout on the school of magic just interrupted) would need on top of the
  bare cancellation this capability provides.
- **No real network transport.** Hosts talk in-process (spec §7: "Host Communication... in-process calls... test
  harness integration" are all legitimate), but the wire *encoding* itself is real (see Property replication
  above) — this is not a shortcut around serialization, only around actual sockets/connections.
- **Health's wire encoding is generic now, but only for Health.** Issues #18 and #21 built the reflection-driven
  generic property codec this bullet used to call a "next step" - `demo/tests/simulated_host.hpp`'s
  `replicate_health_to` now goes through `atlas::replication::write_property_id`/`write_property_fields` (see
  `atlas-replication`'s own README), not a hand-written `health`-specific function, and the codec recurses into
  struct-typed fields (`EntityRef`, `ResourceId`, `PropertyId`, or any other plain struct of supported fields),
  not just primitives - `Health` itself just doesn't happen to have one (its two fields are both `int32`).
  `health::write_health`/`read_health` themselves still exist (`health_test.cpp` still exercises them directly)
  - deleting them, and proving the generic path against a second property, is the natural follow-up once one
  worked example alone isn't the only evidence. *Composed*-property replication strategies (replicate
  contributions vs. resolved effective value, spec §20) remain a wholly separate, still-deferred concern this
  codec doesn't touch.
- **`auto_attack`'s `attack_speed_ticks` doesn't get a haste hook.** `haste` targets `cast_time_attack::CastSpeed`
  only - `auto_attack`'s swing cycle would need the identical `AttackSpeed` composed property plus its own
  `refresh_attack_speed_with_transient_contributions` before a haste source could speed up melee/ranged swings
  the same way it speeds up casts. Deliberately not built speculatively from one example; a second real caller is
  what would justify generalizing `haste` itself to target more than one property, the same reasoning
  `attack_resolution` was only extracted once `cast_time_attack` gave it a second real caller.
- **No animation actually renders anywhere.** `CastStarted`'s `animation`/`duration_ticks` are handed to whatever
  subscribes - proving the resource-identity-plus-duration contract is enough for a real client, without this
  demo building (or needing) a rendering/animation-blending system of its own, matches this demo's own scope
  boundary (see the project root `README.md`'s "never understands... quests, or game rules").
- **No enum manifest field type, no damage range, no multi-state animation.** All three are real gaps
  surfaced by the Fireball worked example (see "Damage over time, and Fireball" above), not attempted here:
  `damage_type` would need `atlas-cgen` to support an enum field type it doesn't have yet; a random damage
  range would need `atlas::core::Random` (already exists, deterministic) wired into a resolve step that
  doesn't exist yet; multiple animation states (wind-up/loop/impact, not just one `animation`/`duration_ticks`
  pair) would need either more `CastStarted` fields or a real animation-state resource concept. Fireball's own
  numbers were chosen specifically so the worked example wouldn't need any of the three.
- **`damage_over_time` has no stacking and no multi-application catch-up.** A single `DotEffect` slot per
  target - a second `ApplyDotEffect` discards whatever was left of a first rather than layering onto it - and
  `AdvanceDotEffect` fires at most one application per call no matter how large `delta_ticks` is, the same
  "a single step large enough to blow straight past... is not detected or corrected" limitation `pathing`'s own
  arrival-epsilon check already documents for itself.
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
