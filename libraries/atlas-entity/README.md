# atlas-entity

**Status:** Seeded. Implements `atlas::EntityRef` (`include/atlas/entity/entity_ref.hpp`) — the entity-identity value type referenced as `atlas::EntityRef` by generated request/event contracts (§21) — and `atlas::entity::EntityRegistry` (`include/atlas/entity/entity_registry.hpp`), a generational-index entity manager (`create()`/`destroy()`/`is_alive()`) that detects stale references after an index is recycled. Nothing else in this library's eventual scope (broader lifecycle hooks, entity-level reflection integration, etc.) is implemented yet.

**Provides:** entity identity, entity lifecycle, entity management mechanisms.

**Spec:** [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities) (responsibility), [§5 Dependency Model](../../docs/specification/05-dependency-model.md) (dependency rules), [§21 Worked Example](../../docs/specification/21-worked-example.md) (`atlas::EntityRef` usage in a generated contract)

## Namespace note

`EntityRef` lives in the top-level `atlas` namespace, not `atlas::entity` — it's a fundamental runtime vocabulary type used directly as `atlas::EntityRef` throughout generated contracts (§21), even though the header is physically owned by this library. `EntityRegistry`, a library-internal mechanism with no equivalent cross-library vocabulary role, lives in `atlas::entity` as usual.

## Dependency position

`atlas-entity` depends only on `atlas_project_options`/`atlas_project_warnings` and the standard library so far — no dependency on `atlas-core` yet, since nothing in `atlas-core` is needed by the current scope. Per §5, it may depend on lower-level libraries and generated contracts as those needs arise, never upward on capabilities, applications, or editor/deployment-specific code.
