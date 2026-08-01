# atlas-entity

**Status:** Seeded. Implements `atlas::EntityRef` (`include/atlas/entity/entity_ref.hpp`) — the entity-identity value type referenced as `atlas::EntityRef` by generated request/event contracts (§21) — and `atlas::entity::EntityRegistry` (`include/atlas/entity/entity_registry.hpp`), a generational-index entity manager (`create()`/`destroy()`/`is_alive()`) that detects stale references after an index is recycled. `EntityRegistry` also exposes entity-creation/destruction lifecycle hooks (`created_since_last_poll()`/`destroyed_since_last_poll()`/`clear_lifecycle_events()` — see "Lifecycle Hooks" below) so interested code can react to an entity going away without `EntityRegistry` knowing what that code is or why it cares. Also provides a `std::hash<atlas::EntityRef>` specialization, added when `atlas-runtime`'s `PropertyStore<T>` needed `EntityRef` to key a `std::unordered_map` — combines both fields (index, generation) via XOR-with-a-shifted-generation, since (unlike `atlas::ResourceId`'s single already-random hash field) neither field alone is well-distributed on its own. Nothing else in this library's eventual scope (entity-level reflection integration, etc.) is implemented yet.

**Provides:** entity identity, entity lifecycle, entity management mechanisms.

**Spec:** [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities) (responsibility), [§5 Dependency Model](../../docs/specification/05-dependency-model.md) (dependency rules), [§21 Worked Example](../../docs/specification/21-worked-example.md) (`atlas::EntityRef` usage in a generated contract)

## Namespace note

`EntityRef` lives in the top-level `atlas` namespace, not `atlas::entity` — it's a fundamental runtime vocabulary type used directly as `atlas::EntityRef` throughout generated contracts (§21), even though the header is physically owned by this library. `EntityRegistry`, a library-internal mechanism with no equivalent cross-library vocabulary role, lives in `atlas::entity` as usual.

## Lifecycle Hooks

`EntityRegistry` records two lists as a side effect of `create()`/`destroy()`:

- `created_since_last_poll()` — every `EntityRef` returned by `create()` since the last clear
- `destroyed_since_last_poll()` — every `EntityRef` that `destroy()` successfully removed since the last clear (a failed `destroy()` — already-dead, stale generation, unknown ref — records nothing, matching `destroy()`'s own "no-op on an invalid ref" contract)

Both are exposed as `std::span<const EntityRef>`, in call order, and both keep accumulating until the caller calls `clear_lifecycle_events()`. There is no implicit tick boundary inside `EntityRegistry` — it has no notion of a tick at all — so a caller that polls once per tick and clears immediately after sees exactly that tick's creations/destructions, the same shape a triggered property gives its reader.

### Design decision: poll-based data, not a callback/subscriber mechanism

Issue #105 asked for "a hook/notification mechanism for entity creation and destruction... [following] the same 'uniform event channel' posture as other runtime notifications (§6) rather than inventing a bespoke callback convention." Two shapes were considered:

1. **Callback/observer list** — `EntityRegistry` holds `std::function<void(EntityRef)>` subscribers, invoked synchronously inside `create()`/`destroy()`. Rejected. This repository's own recent history (the triggered-property migration of `movement::PositionChanged`/`interruption::ActionInterrupted`, previously dispatched via `Context::publish`/`subscribe`) moved *away* from exactly this shape, in favor of a same-tick write a consumer reads during its own scheduled turn — see spec §20, "Triggered composition has no separate delivery mechanism": *"a triggered property's occurrence is not pushed to interested capabilities through a callback or subscriber list... this is what makes triggered composition schedulable... with no callback whose execution order the scheduler cannot see into."* A callback fired from inside `destroy()` has exactly the problem that quote calls out: its execution order relative to everything else running that tick is whatever `destroy()`'s call site happens to be, invisible to any scheduler.
2. **Poll-based data** (chosen) — `EntityRegistry` just accumulates what happened; a caller (e.g. a property-store cleanup pass, itself running during its own scheduled turn) reads `destroyed_since_last_poll()` the same way it would read any other consumed value, then the registry's owner clears it once consumed. This is the same "read once, then discard" lifecycle a triggered property already has, expressed directly as `EntityRegistry`'s own data rather than through `atlas-runtime`'s `PropertyStore`/`Context` machinery — `atlas-entity` has, and should keep, no dependency on either (see "Dependency position" below), so it cannot register itself as a triggered property the way a capability's own manifest-declared properties do. Exposing the equivalent shape as plain data is the smallest mechanism consistent with that boundary: no observer list, no dispatch order for a scheduler to reason about, just two vectors a caller polls and clears on its own cadence.

This also matches CLAUDE.md's Rule of Zero guidance: the two event lists are additional state `EntityRegistry` already protects consistently with its existing slot/free-list invariant (both are `noexcept`-adjacent bookkeeping alongside `create()`/`destroy()`, not a second encapsulated type), so they were added as private members with `[[nodiscard]]` accessors rather than a new collaborator class.

One caller-visible tradeoff worth calling out explicitly: because clearing is caller-driven rather than tied to a tick `EntityRegistry` itself doesn't know about, an owner that never polls (or polls but forgets to clear) simply accumulates the full creation/destruction history for the registry's lifetime. That's an accepted consequence of keeping `EntityRegistry` free of any tick/scheduling dependency, not an oversight — whoever composes an `EntityRegistry` into a host is expected to poll-and-clear on the same cadence it already drives everything else per tick.

## Dependency position

`atlas-entity` depends only on `atlas_project_options`/`atlas_project_warnings` and the standard library so far — no dependency on `atlas-core` yet, since nothing in `atlas-core` is needed by the current scope. Per §5, it may depend on lower-level libraries and generated contracts as those needs arise, never upward on capabilities, applications, or editor/deployment-specific code.
