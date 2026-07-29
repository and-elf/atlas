# Integration test: combat scenario

A true integration test, not a unit test: composes two independent, mutually-unaware capabilities (`health`,
`armor`) into three hand-composed hosts (one authoritative server, two observing clients) and drives the exact
scenario spec §21's worked example describes — a client-issued request, server-side validation, property
composition, authoritative mutation, and replication to observing clients — end to end, proving the request
dispatch, property composition, and replication mechanisms actually work *together*, not just individually in
isolation the way each library's own unit tests already prove.

**Scenario:** client A issues `ApplyDamage(target=B, amount=10)`. B has 10 `Health` and an `Armor` property
composed (Additive, spec §20) from a single +5 contribution. The server validates authority, resolves `Armor`'s
effective value through composition, computes `10 - 5 = 5` effective damage, mutates `Health` to 5, and
replicates the result — over a genuinely serialized wire, not shared memory — to both client A and client B.
Both clients read `Health.current == 5`.

**Why this doesn't belong as a new shipped library:** `Health` and `Armor` are gameplay semantics, and Atlas is
explicit that it "never understands players, health, weapons, inventories, quests, or game rules" (`CLAUDE.md`).
The capability implementations here (`capabilities/health.cpp`, `capabilities/armor.cpp`) and their manifests
(`fixtures/health.capability.yaml`, the shared `tests/fixtures/armor.capability.yaml`) exist only to prove the
underlying mechanism — the same role `atlas-contracts`' own tests already play in reproducing §21's shapes as
fixtures, not real library code.

## What this proves, concretely

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

## What this deliberately does *not* build (and why)

Building all of §7/§8/§20/§6 in full, in one round, would be a separate epic on its own — each of the following
is a real, sizable feature this test intentionally stays inside a smaller boundary around:

- **No manifest-driven capability composition.** A real Atlas host is assembled by tooling from capability
  manifests, resolving `depends_on` into a graph (§7, §8). That generator doesn't exist yet. `SimulatedHost`
  (in `combat_scenario_test.cpp`) hand-composes `health` and `armor` into a host directly in C++ — the same
  scope boundary `atlas-runtime`'s own `Host` already draws around itself ("the hand-composed runtime substrate
  such a manifest-driven host would eventually sit on top of").
- **Only one composition strategy (Additive).** Spec §20 names seven; only Additive has a working evaluator
  (`atlas::runtime::resolve_additive`). The other six (Multiplicative, Override, Priority Override, Set Union,
  Ordered Composition, Weighted Composition) each have genuinely different resolution semantics that don't
  generalize from Additive, and are each their own future increment.
- **No standing, general-purpose contribution registry.** `atlas::armor::ContributionRegistry` is `armor`'s own
  private per-entity bookkeeping (owned per-host, passed explicitly to `add_contribution` — never a namespace-
  scope global, which would silently collide between independently-created `Host` instances allocating entity
  indices from 0 each). A real capability system would likely want a generic version of this in `atlas-runtime`
  itself; building that generalization ahead of a second real use case would be speculative.
- **No client-side prediction or reconciliation** (spec §6). Client A, in this test, simply waits for the
  server's replicated result — it never locally guesses `Health`'s outcome before the server confirms it.
- **No real network transport.** Hosts talk in-process (spec §7: "Host Communication... in-process calls... test
  harness integration" are all legitimate), but the wire *encoding* itself is real (see Property replication
  above) — this is not a shortcut around serialization, only around actual sockets/connections.
- **Health's wire encoding is hand-written, not generic.** `atlas-reflection`'s new `for_each_field` makes a
  reflection-driven generic property codec genuinely buildable now (it wasn't before this round) — but this
  test hand-writes `write_health`/`read_health` directly on `atlas-serialization` primitives, mirroring
  `atlas-replication`'s existing `EntityRef`/`ResourceId` codec precedent. A generic version is a great, now-
  unblocked next step, not attempted here.
- **`RequestResult` chaining uses `.transform(...).value_or(...)`, not spec §21's own `.or_else(...).and_then(...)`
  pseudocode.** Spec §21 explicitly flags its own code as "illustrative pseudocode, not a literal Atlas API
  surface." `std::optional<T>::or_else`/`and_then` (C++23) both require the callback to return another
  `std::optional` of the same value type — chaining directly into a `RequestResult` return (a different type)
  doesn't type-check that way. `.transform(...)` (mapping to a `RequestResult` on the success path)
  `.value_or(...)` (supplying the rejection on the empty path) is the real, compiling C++23 shape of the same
  monadic idea.

## Files

- `fixtures/health.capability.yaml` — this scenario's own copy of the `health` manifest (`depends_on: [entity,
  armor]`, specific to this test), rather than reusing the shared `tests/fixtures/health.capability.yaml` other
  suites use for their own, unrelated purposes.
- `capabilities/armor.hpp`/`.cpp`, `capabilities/health.hpp`/`.cpp` — the manual implementations (spec §14) a
  capability author writes by hand against each generated contract.
- `combat_scenario_test.cpp` — the scenario itself.
