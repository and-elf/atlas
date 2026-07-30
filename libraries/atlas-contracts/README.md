# atlas-contracts

**Status:** Seeded. Implements the compile-time predicates a hand-written or generated contract struct must
satisfy — `atlas::PropertyContract<T>`, `atlas::RequestContract<T>`, `atlas::EventContract<T>`
(`include/atlas/contracts/contract_concepts.hpp`) — plus `atlas::contracts::ContractVersion`
(`include/atlas/contracts/contract_version.hpp`), the exact-match host contract version compared at connection
time (§6, Contract Version Enforcement), built on `atlas::core::SemanticVersion`. Also implements
`atlas::Composition` (the fixed enum of composition strategies §20 names) and `atlas::Composable<T>` (a
`PropertyContract` that additionally names its strategy via a `static constexpr auto composition` member,
matching §20's generated-contract example) — the compile-time vocabulary a composed property's contract needs;
the composition *engine* that actually evaluates contributions against a strategy lives in `atlas-runtime`, not
here (this library only defines what a contract can *say* about itself, never behavior, per §14's Declarative
Boundary). Also implements `atlas::Triggered<T>` (a `PropertyContract` that names itself absent-by-default via a
`static constexpr bool is_triggered` member, matching §20's Triggered composition text: an occurrence read via
the ordinary `ctx.get<T>()` every other property already uses, absent again once the tick that wrote it ends) —
independent of `Composable`, since §20 notes a triggered composition still resolves through the same strategies
a continuous one does, so a property may be composed, triggered, both, or neither. The manifest-to-C++ generator
itself (`tools/atlas-cgen`) is implemented, and generates `PropertyContract`/`RequestContract`/`EventContract`-
satisfying structs directly against these concepts — reflection/serialization metadata generation beyond that
remains out of scope for this library.

**Scoping decision — why the three concepts share one predicate today:** §21's worked example generates
`Health` (a property), `ApplyDamage` (a request), and `HealthChanged` (an event) as three structurally
identical plain aggregates — none carries a field or marker that says "I am a property" versus "I am a
request." That is a deliberate consequence of Atlas's structural, duck-typed composition philosophy (§5, Tiny
Interface Composability): a contract's *kind* is a fact about which manifest block declared it, not about its
C++ shape. Consequently `PropertyContract<T>`, `RequestContract<T>`, and `EventContract<T>` are currently all
aliases for one shared structural predicate (`ContractStruct<T>`: `std::is_aggregate_v<T> &&
std::semiregular<T>`) rather than three independently-constrained concepts. `tests/atlas-contracts/contract_concepts_test.cpp`
reproduces `Health`/`ApplyDamage`/`HealthChanged` verbatim from §21 to prove the concepts accept the spec's own
ground truth, including the (intentional) cross-satisfaction — e.g. `Health` also satisfies `RequestContract`
today, because nothing in its shape says otherwise. See Open Questions below.

**Provides:** contract definitions (the compile-time concepts contracts are checked against), generated
interfaces (`ContractVersion`, laying groundwork for the version-enforcement side of contract identity), public
schema representation.

**Spec:** [§3 Architectural Definitions](../../docs/specification/03-architectural-definitions.md#contract)
(Contract), [§5 Dependency Model](../../docs/specification/05-dependency-model.md#tiny-interface-composability)
(Tiny Interface Composability), [§6 Server Authority](../../docs/specification/06-server-authority.md#contract-version-enforcement)
(Contract Version Enforcement), [§13 Library Architecture](../../docs/specification/13-library-architecture.md)
(Capability Manifest; Manifest Versioning vs. Contract Version Enforcement), [§14 Generated Contracts](../../docs/specification/14-generated-contracts.md),
[§21 Worked Example](../../docs/specification/21-worked-example.md) (ground truth for generated contract shape).

## Open questions for review

- Whether `PropertyContract`/`RequestContract`/`EventContract` should stay permanently identical (consistent
  with structural typing throughout Atlas), or should diverge once the not-yet-built generator starts attaching
  per-kind reflection/serialization metadata (§14) that only one of the three kinds would carry — at which
  point the concepts could check for that metadata's presence instead of just "is a plain aggregate."
- `ContractVersion` wraps `atlas::core::SemanticVersion` in a distinct struct (rather than a type alias)
  specifically so it can't be silently passed where a manifest's `consumes`/`produces` semver *range* (§13) is
  expected, or vice versa — the two are deliberately different concepts (build-time range resolution vs.
  exact-match connection check) sharing only their underlying representation. Confirm this indirection is worth
  its weight versus just using `SemanticVersion` directly once real call sites (host connection handshake) show
  how it is actually used.
- `check_contract_version`/`ContractVersionMismatch` model only the comparison and diagnostic *data* from §6 —
  there is no host/connection type yet (that belongs to `atlas-runtime`/`atlas-replication`, neither of which
  exist yet), so nothing in this library actually calls it from a real handshake path. Flagging in case the
  intended shape of that eventual integration should influence this data shape now rather than later.

## Dependency position

`atlas-contracts` depends on `atlas-core` (for `atlas::core::SemanticVersion`, which `ContractVersion` wraps)
plus `atlas_project_options`/`atlas_project_warnings` and the standard library. Per §5, generated contracts sit
below capability libraries and above runtime libraries in the dependency graph; this library depends only
downward, consistent with that position. The test suite additionally links `atlas::entity` (test-only, not a
library dependency) so `contract_concepts_test.cpp` can reproduce the §21 worked example's `atlas::EntityRef`
fields verbatim.
