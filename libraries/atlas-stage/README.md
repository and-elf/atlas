# atlas-stage

**Status:** Seeded. Implements `atlas::stage::StageId` (`include/atlas/stage/stage_id.hpp`), a value type naming one step of the runtime's internal execution sequence, and `atlas::stage::StageSequence` (`include/atlas/stage/stage_sequence.hpp`), an encapsulated, immutable, ordered container of `StageId`s that protects one invariant — no duplicate stage — and always iterates in the exact insertion order it was constructed with.

This is deliberately **not** a mechanism capabilities declare themselves into. §5 (Ordering Without Stages) is explicit that capability execution order comes entirely from the `depends_on` graph, with no separate stage/phase concept alongside it. `atlas-stage` is instead the runtime-internal piece that same section points at: "The runtime still executes in a fixed, deterministic sequence internally (`atlas-stage`, §13)." `StageSequence` is that fixed sequence's concrete representation — something the (not-yet-built) runtime assembles once from its own fixed set of internal stages and then walks identically every tick, never something a capability manifest references or contributes to.

**Scoping decisions:**
- `StageId` carries only a `name` (`std::string_view`, expected to reference a string literal / other static-storage-duration string — matching that stage names are fixed by the runtime itself, not assembled dynamically at runtime). No numeric ordinal field: a stage's position is entirely a property of where it sits in a `StageSequence`, not something the ID itself encodes.
- `StageSequence::create` returns `std::optional<StageSequence>` (mirroring `atlas::core::SemanticVersion::parse`) rather than throwing or asserting on a duplicate — construction failure is an ordinary, checkable outcome, consistent with how this codebase treats malformed input elsewhere.
- Duplicate detection sorts a *copy* of the input by name for an O(n log n) check; the sequence's own public iteration order (`begin()`/`end()`) always reflects the caller's original insertion order, never the sort order used internally to detect duplicates. This keeps the "bit-for-bit reproducible iteration order" guarantee tied to what the caller actually supplied, not to a hash- or sort-based reordering.
- What the runtime's actual fixed internal stages are named, and how many there are, is out of scope here — that's a runtime-composition decision for `atlas-runtime` once it exists. This library only provides the vocabulary type and the invariant-protecting container; it does not enumerate or hard-code any concrete stage list.

**Provides:** execution stages, lifecycle organization, deterministic ordering boundaries.

**Spec:** [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities) (responsibility), [§5 Dependency Model](../../docs/specification/05-dependency-model.md#ordering-without-stages) (Ordering Without Stages — why this is runtime-internal, not capability-facing), [§4 Architectural Invariants](../../docs/specification/04-architectural-invariants.md#deterministic-execution) (Deterministic Execution — the fixed-order guarantee this library protects)

## Dependency position

`atlas-stage` depends only on `atlas_project_options`/`atlas_project_warnings` and the standard library — no dependency on `atlas-core` or `atlas-entity` yet, since nothing in either is needed by the current scope. Per §5, it may depend on lower-level libraries and generated contracts as those needs arise, never upward on capabilities, applications, or editor/deployment-specific code.
