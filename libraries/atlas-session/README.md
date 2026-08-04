# atlas-session

**Status:** Seeded (issue #222). Implements `atlas::SessionId` (top-level namespace,
`include/atlas/session/session_id.hpp`) — the session-identity vocabulary type, mirroring
`atlas::EntityRef`/`atlas::ResourceId`'s own top-level-namespace precedent — the
`atlas::session::SessionStore` concept (`include/atlas/session/session_store.hpp`), mirroring
`atlas::render::FrameBackend`/`atlas::audio::AudioBackend`'s own shape, and
`atlas::session::InMemorySessionStore` (`include/atlas/session/in_memory_session_store.hpp`,
`src/in_memory_session_store.cpp`) — the first real backend, genuinely valid for a standalone or
single-shard deployment, not a stub awaiting a "real" implementation later.

**Provides:** session identity (`SessionId`) and lifecycle (`SessionStore`: create, validate,
revoke) backing the origin metadata Request Trust and Permission (§6) depends on.

**Spec:** [§6 Server Authority](../../docs/specification/06-server-authority.md#session-identity)
("Session Identity"), [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities)
(responsibility)

## Namespace note

`SessionId` lives in the top-level `atlas` namespace, not `atlas::session` — it's a fundamental
runtime vocabulary type a capability declares directly as a `SessionId`-typed request field (§6),
even though the header is physically owned by this library. `SessionStore` and
`InMemorySessionStore`, which have no equivalent cross-library vocabulary role, stay in
`atlas::session` as usual.

## Security: SessionId generation

`SessionId` wraps two `std::uint64_t` words (128 bits) rather than a narrower representation, wide
enough that a session identity must never be practically guessable. This is a genuine security
decision, not a default picked casually — see `session_id.hpp`'s own doc comment for the full
rationale, and `in_memory_session_store.hpp`/`.cpp` for how `InMemorySessionStore` actually draws
the bits:

- **Never `atlas::core::Random`.** That type is this project's deterministic *simulation*
  randomness source (spec §4: seeded, reproducible, replayable per host). Reusing it for session
  identity would make sessions guessable to anyone who could infer or replay a host's simulation
  seed — defeating the entire purpose of an unguessable identity.
- **Every word comes directly from `std::random_device`**, not from a `std::mt19937_64`-class
  engine merely *seeded* by one. Drawing every word straight from the OS entropy source avoids a
  different, more subtle weakness: `std::mt19937`'s internal state is fully reconstructable from a
  long enough run of consecutive outputs, which would make every future `SessionId` predictable
  once enough past ones from the same engine had been observed, even with an unguessable initial
  seed. `create_session()` is called once per connection, not once per tick, so the extra entropy
  draws this costs are immaterial next to the security property they buy.

## Scope (issue #222)

- One real backend this round: `InMemorySessionStore`. Not gated behind a configure-time backend
  option the way `atlas-render`/`atlas-audio`/`atlas-physics` gate their real (vs. Null) backends —
  there is no Null/stub variant of `SessionStore` (spec §6 draws this distinction explicitly): an
  in-memory store already *is* a real, correct implementation for the deployment shape it targets.
  A distributed backend is a separate, later issue.
- **`SessionOrigin` (`Public`/`Internal`) is explicitly out of scope this round** — spec §6 already
  flags it as designed against an eventual real listener/transport layer that doesn't exist yet
  (`atlas-replication`'s own README: real network transport is out of scope today).
- No accounts, credentials, or authentication policy of any kind — this library answers only "is
  this session currently valid" (§6, Session Identity: "Mechanism, not meaning").

## Dependency position

`atlas-session` depends only on `atlas_project_options`/`atlas_project_warnings` and the standard
library — no dependency on any other library in this repository. Per §5, it may depend on
lower-level libraries and generated contracts as those needs arise, never upward on capabilities,
applications, or editor/deployment-specific code.
