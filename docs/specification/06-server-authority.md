## 6. Server Authority

Atlas is server authoritative.

- Authoritative simulation executes on server hosts.
- Client hosts observe replicated state and issue requests.
- Replication distributes observable state from authoritative hosts.
- Authority is a responsibility of hosts rather than capabilities.

Capabilities remain independent of deployment topology and may execute within:

- server hosts
- client hosts
- editor hosts
- testing hosts
- other host compositions

without modification.

> A capability defines behavior. A host defines authority.

### Terminology: Request vs. Internal Dispatch

Atlas has one dispatchable contract kind: the **request** (§14, declared under a capability's `requests:` block, generated as a `RequestContract`). "Command" is not a separate schema concept — it describes a specific *origin*, not a different technical mechanism.

- A request **issued by a client** originates at the boundary between a client host and an authoritative host, crosses that boundary, is subject to validation and rejection (below), and may be predicted and reconciled (§6, Request Validation and Reconciliation).
- A request **dispatched internally** — one capability calling into another capability's request contract as part of handling its own work — never crosses the client/server boundary and was never issued by a client. This document calls such an internal dispatch a **command** as a matter of description, to keep prose about "what a capability does with an already-validated request" readable — but it is the same `RequestContract` mechanism, invoked from within a handler rather than from across the network.

An internal command is not itself subject to Request Validation — it inherits whatever validation the client-issued request that triggered it already passed. A client-issued request commonly results in one or more internal commands once it is accepted — but the request is the thing a client sent and a server decided on; the internal commands are how the server's own capabilities then carry that decision out, using the same dispatch mechanism, just invoked from code rather than from across the network.

### Request Validation and Reconciliation

Clients issue requests. A request is not a guaranteed state change — it is honored only if the server validates it.

The server validates every incoming request against authoritative state before applying it. If a request is invalid — because it conflicts with current authoritative state, fails a capability-defined precondition, or is not permitted for the issuing client — the server rejects the request. The server never silently mutates a client's request to make it valid.

```mermaid
sequenceDiagram
    participant Client
    participant Server
    Client->>Client: Predict request locally
    Client->>Server: Issue request
    Server->>Server: Validate against authoritative state
    alt Valid
        Server->>Server: Apply to authoritative state
        Server-->>Client: Confirm / replicate new state
        Client->>Client: Prediction confirmed
    else Invalid
        Server-->>Client: Reject request
        Client->>Client: Reconcile — resimulate from last confirmed state
    end
```

Validation and application both happen at a tick boundary (§4, Tick Execution), never instantly at the moment a request arrives. Every request received since the previous tick accumulates into that tick's input batch and is processed in a fixed deterministic order — never applied early because an application considers it latency-sensitive, and never delayed relative to other requests in the same batch. A constant, predictable per-tick delay is preferable to variable per-request latency: jitter in *when* a request takes effect is a worse experience than a small, fixed delay every request equally incurs, and — more fundamentally — an "instant path" that bypasses the tick boundary would be a second, ad hoc execution order existing alongside the one the dependency graph already defines, which is exactly what Deterministic Execution (§4) rules out.

A rejected request is not applied to authoritative state. The server communicates rejection back to the originating client through the normal contract boundary (requests, events, or replicated state, as appropriate to the capability).

The client is responsible for reconciliation. Because Atlas guarantees bit-exact determinism, a client capability may predict the outcome of a request locally before server confirmation, then reconcile by resimulating from the last confirmed authoritative state if the server's outcome differs from the local prediction.

Reconciliation is a **capability** concern, not a runtime concern. The runtime provides the deterministic execution and replay mechanisms that make reconciliation possible (fixed stage ordering, deterministic resimulation from a known state). Capabilities decide whether and how to predict, and how to resolve a mismatch (e.g., snap, blend, or replay).

This preserves the existing invariant: authority is a responsibility of hosts, and capabilities remain unaware of deployment topology. A capability that predicts locally behaves identically whether it happens to run on a client host reconciling against a remote server or a host running standalone against its own authoritative state.

### Request Trust and Permission

Atlas does not enforce who is permitted to issue a given request. The runtime's responsibility ends at delivery: it routes a request from its origin to the capability that defines it, deterministically and reliably. Whether that request should be honored is a decision the capability makes.

This follows directly from Mechanism Over Meaning (§2). "Who may open this door" or "who may cast this spell" is application semantics, not execution mechanics. Atlas has no concept of a player, a role, or a permission — those concepts belong to capabilities, the same way health and inventories do.

What the runtime does provide is **origin information**: a request arrives with enough identity/origin metadata (which connection, which client, which host) for a capability to make a trust decision. Atlas guarantees this metadata is authentic — a capability can trust that a request claiming to originate from connection X actually did — but Atlas does not interpret it.

Capabilities are responsible for defining and enforcing their own permission model on top of this origin metadata: for example, verifying that the issuing client owns the entity a request targets, or that a request is only valid from a client currently in a particular game state. This is validated as part of the same Request Validation step described above — an unauthorized request is simply an invalid request, and is rejected the same way a request that violates game-state preconditions is rejected.

> Atlas provides no default permission model. A capability that defines no trust policy accepts requests from any origin.

### Runtime Failure Reporting

Runtime failures are reported through a single, uniform error channel, shared across every runtime system.

A runtime failure is any condition where a runtime system cannot complete an operation it was asked to perform:

- a resource fails to resolve
- a replication update cannot be delivered
- a host disconnects unexpectedly
- a request is rejected (see Request Validation and Reconciliation, above)
- a contract version mismatch refuses a connection (see Contract Version Enforcement, below)

```mermaid
flowchart LR
    Resource["Resource System"] --> Channel["Uniform Failure Channel"]
    Replication["Replication System"] --> Channel
    Networking["Networking System"] --> Channel
    Requests["Request System"] --> Channel
    Channel --> Sub1["Capability subscribes"]
    Channel --> Sub2["Capability subscribes"]
```

Rather than each system (resource, replication, networking, scheduling) defining its own bespoke failure signaling, every runtime system reports failures as events on this shared channel, using a common failure structure: what failed, which system reported it, and system-specific detail relevant to diagnosing it (e.g. the resource identifier that failed to resolve, or the two contract versions that mismatched).

Capabilities subscribe to this channel the same way they subscribe to any other event — through the existing event delivery mechanism (§15, Runtime Responsibilities). There is no separate failure-handling API to learn; failures are events like any other, just originating from the runtime rather than application logic.

This keeps failure handling consistent with the rest of the platform's design: one mechanism, reused everywhere, rather than a proliferation of per-system error-handling conventions that capabilities would each need to learn independently.

A capability that does not subscribe to the failure channel simply does not observe runtime failures relevant to it. Atlas does not impose default failure-handling behavior (e.g. automatic retry, automatic disconnection) — the same way it imposes no default permission model. Applications decide what a failure means and how to respond to it.

### Contract Version Enforcement

Every build produces a contract version, derived from the generated contracts a host was compiled against.

A client and server must have matching contract versions to communicate. There is no partial compatibility and no negotiation.

```mermaid
sequenceDiagram
    participant Client
    participant Server
    Client->>Server: Connect (contract version vX)
    Server->>Server: Compare to own version (vY)
    alt vX == vY
        Server-->>Client: Connection accepted
    else vX != vY
        Server-->>Client: Refuse + structured diagnostic (vX, vY)
    end
```

On connection, the server checks the client's contract version against its own. If the versions do not match exactly, the server refuses the connection. No requests are accepted, and no session begins.

This applies uniformly regardless of what changed between versions. A contract version mismatch is refused whether the underlying change was breaking or not — Atlas does not attempt to infer compatibility between versions at connection time.

This keeps the runtime and tooling simple: there is exactly one rule ("versions match, or the connection is refused"), rather than a matrix of partial-compatibility cases to validate and maintain. Any softer compatibility guarantee (e.g. supporting two adjacent versions simultaneously) is an application-level or deployment-level concern, not an Atlas platform guarantee.

Refusal is not silent. When the server refuses a connection due to a version mismatch, it exposes a structured diagnostic containing both the server's contract version and the client's contract version.

This diagnostic is delivered through the normal contract boundary, not as an opaque failure. Applications may surface it to the user in whatever form fits the host (e.g. "client out of date: expected vX, got vY"), route it to logging, or trigger an update flow. Atlas defines the diagnostic's structure and delivery; applications define how it is presented, consistent with the platform's existing division between mechanism and meaning.
