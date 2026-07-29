# atlas-runtime

**Status:** Seeded. Implements `atlas::runtime::Host` (`include/atlas/runtime/host.hpp`) — the concrete runtime object a server or client process actually instantiates to hold state and drive execution. A `Host` owns an `atlas::entity::EntityRegistry` and an `atlas::scheduler::Scheduler` (constructed from an `atlas::stage::StageSequence`), and exposes:

- entity lifecycle: `create_entity()`, `destroy_entity(EntityRef)`, `is_entity_alive(EntityRef) const` — each delegating directly to the owned `EntityRegistry`.
- tick execution: `schedule(const StageId&, Job)`, `run_tick() const`, `sequence() const` — each delegating directly to the owned `Scheduler`.

Nothing else in this library's eventual scope — see Scoping decisions below — is implemented yet.

**Scoping decisions:**

- This is deliberately **not** the eventual capability-composed host described in §7 (Host Composition) and §8 (Atlas Hosts) — that version is assembled by Atlas tooling from capability manifests (`depends_on`, `contracts.consumes`/`produces`, §13/§14), and neither the manifest format's code-generation step nor a host manifest reader exists in this repository yet. Building or hand-simulating that layer was explicitly out of scope for this slice. `Host` is instead the hand-composed runtime substrate such a manifest-driven host would eventually sit on top of: the concrete object a process new()'s up today, composed only from systems that already exist for real in this repo (`atlas-entity`, `atlas-stage`, `atlas-scheduler`).
- `Host` is an encapsulated class (private members + delegating methods), not a plain aggregate, even though it adds no invariant of its own beyond what `EntityRegistry` and `Scheduler` already protect internally: its entire reason to exist is to be the single owner composing those two systems, so there is no public field a caller could usefully set directly without going through one of the delegating methods.
- `schedule()`/`run_tick()`/`sequence()` are thin pass-throughs to the owned `Scheduler`, and `create_entity()`/`destroy_entity()`/`is_entity_alive()` are thin pass-throughs to the owned `EntityRegistry` — `Host` adds no additional validation or business logic beyond delegation. This matches the assignment's framing of `atlas-runtime` at this stage: proving out "coordination between systems" using only what already concretely exists, not inventing new behavior ahead of the systems that would need it (request routing, replication, resource resolution — none of which exist yet either).
- **Authority (server vs. client) is explicitly left undecided** — see Open questions below.

**Open questions (flagging for human review, not silently resolved):**

- §6 (Server Authority & Requests) states "authority is a responsibility of hosts," but does not specify *how* a `Host` should represent or enforce that responsibility. Whether authority becomes a flag on `Host`, a distinct `ServerHost`/`ClientHost` type, a capability-level concern layered on top, or something else entirely is left for a human to decide once `atlas-request` (request routing/validation) and `atlas-replication` (state synchronization) have more concrete shape to design against — guessing at a shape now risked baking in a wrong answer that later work would have to unwind.
- Composing capabilities, resolving `contracts.consumes`/`produces`, and reading a host manifest are all out of scope here by design (per the assignment) — `Host` has no notion of a capability graph at all yet. When the manifest-to-C++ generator (`tools/generators/`) exists, it's an open question whether `Host` itself grows a capability-composition entry point or whether a separate generated layer sits on top of it unchanged.

**Provides:** host execution environment, runtime integration, coordination between systems.

**Spec:** [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities) (responsibility), [§7 Host Composition](../../docs/specification/07-host-composition.md) (hosts as logical execution contexts, not OS processes), [§8 Atlas Hosts](../../docs/specification/08-atlas-hosts.md) ("every host shares the same runtime architecture"), [§15 Runtime Libraries](../../docs/specification/15-runtime-libraries.md) (runtime responsibilities vs. application semantics), [§6 Server Authority](../../docs/specification/06-server-authority.md) (authority as a host responsibility — the open question above)

## Dependency position

`atlas-runtime` depends publicly on `atlas::entity`, `atlas::scheduler`, and `atlas::stage` (all three appear directly in `Host`'s public interface — its constructor takes a `stage::StageSequence`, its entity methods take/return `EntityRef`, and `schedule()` takes a `scheduler::Job`), plus `atlas_project_options`/`atlas_project_warnings` and the standard library. Per §5, it may depend on further lower-level libraries and generated contracts as those needs arise, never upward on capabilities, applications, or editor/deployment-specific code — consistent with `atlas-runtime` sitting near the top of the runtime-library stack, coordinating the systems beneath it rather than being coordinated by them.
