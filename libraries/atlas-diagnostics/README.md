# atlas-diagnostics

**Status:** Seeded (issue #82). Implements the structured logging/diagnostics mechanism spec §6 (Server
Authority, "Runtime Failure Reporting") describes but that no library in this repository previously
built: `atlas::diagnostics::Severity` + `to_string(Severity)` (`include/atlas/diagnostics/severity.hpp`,
`src/severity.cpp`), `atlas::diagnostics::Record` + `atlas::diagnostics::DetailField`
(`include/atlas/diagnostics/record.hpp`) — spec §6's "common failure structure: what failed, which
system reported it, and system-specific detail" as a concrete type — `atlas::diagnostics::Sink`
(`include/atlas/diagnostics/sink.hpp`), the pluggable output interface, `atlas::diagnostics::ConsoleSink`
(`include/atlas/diagnostics/console_sink.hpp`, `src/console_sink.cpp`), the one concrete sink this round
ships, and `atlas::diagnostics::Logger` (`include/atlas/diagnostics/logger.hpp`), which fans a `Record`
out to every registered `Sink`.

This library provides the **mechanism** only — a record shape and a way to route it to output. It does
not itself report any runtime failure (resource resolution, replication delivery, disconnects, request
rejection, contract version mismatch); wiring each of those runtime systems to actually call
`Logger::log()` at the moment they fail is explicit, separate follow-up work once this mechanism exists
(see "Kept decoupled from any specific runtime failure type" and "Open questions" below).

## Why `atlas-diagnostics`, not `atlas-log`

Both were live options (the issue names either as acceptable). `atlas-request`'s own
`request_diagnostics.hpp`/`request_diagnostics.cpp` (`describe(const RequestResult&)`) already anticipates
this library by name in its own header comment — "e.g. for the uniform runtime failure channel (§6...)
once atlas-runtime exists to actually publish request rejections onto it" — and the issue's own title is
"Structured logging / **diagnostics** framework." `atlas-log` would describe only the console/file output
half of what this library does; `atlas-diagnostics` covers both the structured-record vocabulary
(`Record`/`Severity`/`DetailField`) and the output mechanism (`Sink`/`Logger`) under one name that already
has a foothold in this codebase's own vocabulary.

## Scoping decisions

- **`Record` generalizes spec §6's "failure structure" slightly, to also cover non-failure diagnostics.**
  §6 frames the shared structure specifically as a failure structure, but the same three-part shape (what
  happened, which system, what detail) is equally useful for a system wanting to record an ordinary,
  expected event — a resource resolving successfully after a retry, a tick starting — not only a failure.
  `Severity` carries that distinction (`Debug`/`Info` vs. `Warning`/`Error`/`Critical`) rather than this
  library inventing a second, parallel non-failure record type; a reporting system chooses the severity
  that fits what actually happened.
- **Five `Severity` levels**, not a single generic "failure" flag: the runtime failures spec §6 lists span
  a real range of severity — a client-issued request being rejected is an ordinary, expected outcome of
  validation (§6, Request Validation and Reconciliation), materially different from a host disconnecting
  unexpectedly or a contract version mismatch refusing a connection outright. Collapsing all of them to
  one severity would discard exactly the distinction a future filtering/replay tool (see Open questions)
  needs between "routine, worth recording" and "something is actually broken."
- **`details` is an ordered `std::vector<DetailField>`, not a `std::unordered_map<std::string, std::string>`.**
  Iteration order over an `unordered_map` is a standard-library implementation detail that can differ by
  hash seed — not a determinism problem here, since a diagnostic `Record` is deliberately kept out of
  simulation state (see "Deterministic-safe" below), but it would still undermine this library's own
  stated purpose: structured, queryable output a future tool greps/diffs, and tests asserting exact
  rendered text. A vector preserves whatever order the reporting system inserted its details in, at the
  cost of O(n) lookup by key — a cost nothing here pays, since every consumer (`ConsoleSink`, a future
  file sink) iterates the full set rather than looking up one detail among many.
- **`DetailField::value` is always a `std::string`**, not a typed variant. A reporting system with, say, a
  numeric resource identifier renders it (`std::to_string`, or whatever formatting it already uses) before
  handing it here. This keeps `Record` itself simple and matches its purpose: a diagnostic payload exists
  to end up as human/tool-readable text, not to round-trip as a typed value the way a replicated property
  does.
- **`Sink` is a small polymorphic base class, not a `std::function<void(const Record&)>`** — unlike
  `atlas::scheduler::Job`, which *is* a bare `std::function<void()>`. A sink typically owns state that
  persists across many `write()` calls (an output stream today; an open file handle for a future file
  sink), which reads more naturally as a named type a caller constructs once and registers than as a
  capturing lambda sharing that state awkwardly. This is a plain runtime plugin point, not a capability
  contract, so spec §5's "never a runtime interface table or virtual dispatch lookup" — which governs
  compile-time capability-to-capability contract satisfaction — does not apply to it.
- **Only `ConsoleSink` ships this round**, per the issue's explicit YAGNI instruction ("don't over-build
  anything beyond what's needed to prove the mechanism works"). `tests/atlas-diagnostics/logger_test.cpp`
  proves `Logger`'s dispatch mechanism using a `CapturingSink` defined *only* in that test file (a small
  `Sink` subclass collecting `Record`s into a `std::vector`) rather than shipping a production in-memory
  sink — proof that the interface is straightforward to extend without this library actually building the
  extension yet. A future file sink or a shipped, reusable in-memory test sink are natural next
  increments, not attempted here.
- **`ConsoleSink` takes its target `std::ostream&` by constructor argument, defaulting to `std::cout`**
  (CLAUDE.md, Architecture Principles: dependency injection) — this is what makes testing its exact
  rendered output format straightforward (`tests/atlas-diagnostics/console_sink_test.cpp` constructs one
  against a `std::ostringstream`) without capturing real stdout in a test.
- **`Logger::add_sink()` borrows a `Sink&`, the same non-owning-reference pattern
  `atlas::Context::register_property_store<T>()` already establishes** — whoever composes a host owns the
  actual `Sink` instances and decides their lifetime; `Logger` only coordinates dispatch to them.
- **`Logger` dispatches to sinks strictly in registration order**, never an unordered container — spec §4,
  Deterministic Execution: "avoid unordered iteration... anywhere it could affect output," applied here
  even though sink dispatch order can't affect *simulation* output (see next section) — it can still
  affect a test asserting exact output across multiple sinks, or a future scenario stacking sinks with
  side effects that must happen in a defined order (e.g. a file sink before a network sink).
- **Kept decoupled from any specific runtime failure type.** `Record::system` is a plain `std::string`
  (e.g. `"resource"`, `"replication"`, `"request"`), not an enum enumerating every runtime system that can
  report — this library provides the record shape and the sink mechanism, never the taxonomy of what can
  fail. Wiring an actual runtime failure site (a rejected request, a resource that failed to resolve, a
  replication delivery failure, an unexpected disconnect, a contract version mismatch) to call
  `Logger::log()` is each runtime library's own concern and explicit future follow-up — out of scope for
  this round by the issue's own instruction to stay inside this library's own directory.

## Deterministic-safe: presentation/diagnostic-only, never fed back into simulation

Spec §4 requires bit-exact determinism: identical inputs must produce identical outputs, down to the bit,
across machines and full-session replay. Logging a `Record` is explicitly **not** part of that guarantee
and must never become part of it:

- `Logger::log()`/`Sink::write()` are one-way, write-only operations. `Logger` deliberately exposes no way
  to query what has already been logged (no "read the log back" method of any kind) — there is no read
  path for simulation logic to accidentally come to depend on.
- Nothing in this library reads platform wall-clock time, and `Record` carries no timestamp field this
  round (kept out deliberately — see Open questions) — so there is no wall-clock value flowing through
  this library that a future addition would need to remember stays presentation-only (spec §4's carve-out:
  "wall-clock time may be used only for presentation-only concerns... and must never feed back into
  simulation state").
- `ConsoleSink` performs ordinary, unbuffered-relative-to-simulation I/O (writing to a `std::ostream`) —
  the same category of operation spec §4 already permits for audio/render interpolation, never something
  a request handler, property-composition resolver, or scheduler job should read the result of.
- A capability author should treat every call into this library the same way: fire-and-forget. If a
  future capability is ever tempted to branch simulation behavior on "was anything logged this tick," that
  is the architectural defect CLAUDE.md's Determinism Constraints section calls out explicitly, not a
  legitimate use of this mechanism.

## Provides

Structured diagnostic/log records (`Record`, `Severity`, `DetailField`), a pluggable sink interface
(`Sink`) with one concrete console/stdout sink (`ConsoleSink`), and a fan-out dispatcher (`Logger`) —
the mechanism spec §6's uniform runtime failure channel is meant to route through, once each runtime
system is wired to call it. Provides no taxonomy of what can fail and no wiring into any specific runtime
failure site — see "Kept decoupled from any specific runtime failure type" above.

**Spec:** [§6 Server Authority](../../docs/specification/06-server-authority.md#runtime-failure-reporting)
("Runtime Failure Reporting" — the uniform failure channel this library provides the developer-observable
mechanism for; "a common failure structure: what failed, which system reported it, and system-specific
detail" is `Record`'s three fields directly), [§4 Architectural Invariants](../../docs/specification/04-architectural-invariants.md)
(Deterministic Execution — why this library documents itself as presentation/diagnostic-only), [§13 Library
Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities) (this library is
not yet listed in §13's responsibility table — flagged in Open questions below, since adding a new row
there is a spec change outside this round's directory-scoped instructions)

## Dependency position

`atlas-diagnostics` depends only on the standard library, plus the shared `atlas_project_options`
(PUBLIC) / `atlas_project_warnings` (PRIVATE) interface targets — the same CMake pattern `atlas-core`
establishes. It sits at the same foundational level as `atlas-core`: nothing in this library depends on
`atlas::entity`, `atlas::runtime`, or any other library in this repository, and per §5 every other library
may depend downward on it once a real call site wires a runtime failure through `Logger`. Nothing outside
this library's own directory, its tests, or the two `add_subdirectory()` registrations in
`libraries/CMakeLists.txt`/`tests/CMakeLists.txt` was touched to build it — no existing runtime failure
site (`atlas-request`, `atlas-resource`, `atlas-replication`, ...) was wired to call `Logger::log()` yet,
by design (see "Kept decoupled from any specific runtime failure type" above).

## Open questions (flagging for human review, not silently resolved)

- **No timestamp field on `Record` this round.** A real console/log line arguably wants one, but adding it
  raises exactly the question "where does the clock value come from" this library's Deterministic-safe
  section above takes care to avoid — `atlas::core::Time` is a tick count meaningful only within a running
  host's own simulation, not a wall-clock value meaningful in a log line read by a human across multiple
  runs, and reading real wall-clock time is the one thing this library was careful not to need. Whether a
  future round adds a wall-clock timestamp (explicitly presentation-only, per §4's carve-out) or a tick
  count (meaningful only alongside the host that produced it) — or both — is left open rather than guessed
  at now, since nothing in this round's acceptance criteria required it.
- **No sequence number / correlation id.** A future deterministic replay tool (explicitly named in the
  issue as a motivating future consumer) will likely want to correlate a logged `Record` back to the exact
  tick and/or job that produced it. Nothing in `Record` carries that yet — it wasn't part of this round's
  stated minimum (severity, system, message, extensible detail), and guessing at a correlation scheme
  ahead of the replay tool that would actually consume it risks building the wrong shape.
- **§13's library responsibility table does not list `atlas-diagnostics` yet.** This round's instructions
  scoped changes to this library's own directory, its tests, and the two `CMakeLists.txt` registrations —
  updating `docs/specification/13-library-architecture.md`'s table (and the root `CLAUDE.md`/README
  repository-layout listings) to add a row for this library is a natural, small follow-up, deliberately
  not done here to stay inside that scope.
- **No file sink or shipped in-memory sink yet** — see "Only `ConsoleSink` ships this round" above. Both
  are straightforward extensions of `Sink` once a real consumer needs them (a file sink for persistent
  logs; a shipped, reusable in-memory sink so other libraries' tests don't each need their own
  `CapturingSink`-style helper) but neither was needed to prove this mechanism works.
- **No wiring into any real runtime failure site yet.** `atlas-request`'s rejection path, `atlas-resource`'s
  resolution failures, `atlas-replication`'s delivery failures, and connection-time contract version
  mismatches (§6, Contract Version Enforcement) are the concrete future callers of `Logger::log()` spec §6
  describes — none of them call it yet. This is explicit, out-of-scope follow-up per the issue's own
  instructions, not an oversight.
