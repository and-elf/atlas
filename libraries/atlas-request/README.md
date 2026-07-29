# atlas-request

**Status:** Seeded. Implements `atlas::RequestResult` (`include/atlas/request/request_result.hpp`) — the
outcome type every request handler returns — plus the two constrained factory functions that produce one,
`atlas::accept(cmd)` and `atlas::reject(cmd, reason)`, exactly as spelled in §21's worked example
(`return atlas::accept(cmd);` / `return atlas::reject(cmd, "not authoritative");`). Both are templates
constrained by `atlas::RequestContract<T>` (from `atlas-contracts`), so only a contract-shaped request type can
be accepted or rejected through this path — a type that isn't a `RequestContract` (e.g. one with a
user-declared constructor or a non-copyable member) fails to compile at the call site. Also implements a small
diagnostic helper, `atlas::request::describe(const RequestResult&)`
(`include/atlas/request/request_diagnostics.hpp`), rendering a result as `"accepted"` or
`"rejected: <reason>"`. Nothing else in this library's eventual scope (actual request dispatch/routing from a
network or capability boundary to a handler, §6 Terminology: Request vs. Internal Dispatch) is implemented
yet — that depends on the not-yet-built manifest generator and host/capability composition runtime
(`atlas-runtime`), neither of which exist in this repository yet, so building either was explicitly out of
scope for this slice.

**Scoping decisions:**

- `RequestResult` is a basic aggregate (rule of zero): `accepted` plus `rejection_reason` (empty when
  accepted). Nothing about it protects an invariant across its own operations — `accept()`/`reject()` are the
  intended way to produce one, but there is no reason to hide the fields behind an encapsulated class for that
  alone.
- `accept(cmd)`/`reject(cmd, reason)` take the request *value* only to drive template argument deduction and
  the `RequestContract<T>` constraint — per §6 (Request Trust and Permission / Request Validation), acceptance
  or rejection is a decision about the request's *validity*, not something derived from its content, so the
  value itself is never inspected.
- `reject()` requires a `std::string` reason and stores it verbatim, never falling back to a canned message
  when given an empty string. §6 is explicit that "the server never silently mutates a client's request to
  make it valid" — extending that same principle to the reason itself (never second-guessing or replacing what
  the caller wrote) keeps rejection genuinely explicit end to end.
- `RequestResult` and `accept()`/`reject()` live in the top-level `atlas` namespace, not `atlas::request` — the
  same reasoning as `atlas::EntityRef` (`atlas-entity`'s README, Namespace note): this is cross-library
  vocabulary named directly in every capability's generated request-handler signature (§21), even though the
  headers are physically owned by this library. `describe()`, a library-internal diagnostic helper with no
  cross-library vocabulary role, lives in `atlas::request` as usual.
- `describe()` is a small, genuinely out-of-scope-adjacent addition: it does not implement the uniform runtime
  failure channel (§6, Runtime Failure Reporting) — that channel belongs to `atlas-runtime`, which does not
  exist yet — it only provides the rendering logic that channel will eventually want for a rejected request. It
  is included because it is real, testable behavior that stays entirely within this library's own data, rather
  than speculative dispatch/routing infrastructure.

**Provides:** request definitions (`RequestResult`), request execution infrastructure (`accept`/`reject`, the
explicit accept-or-reject-with-reason primitive request handlers use), a small request-outcome diagnostic
helper. Request *routing* (delivering a request from a network or internal-dispatch origin to a handler) is
out of scope for this slice — see Status above.

**Spec:** [§6 Server Authority](../../docs/specification/06-server-authority.md#request-validation-and-reconciliation)
(Request Validation and Reconciliation — "the server never silently mutates a client's request to make it
valid"; Request Trust and Permission — acceptance/rejection is a validity decision, not content-driven; Runtime
Failure Reporting — the eventual consumer of `describe()`), [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities)
(responsibility; the atlas-request row), [§21 Worked Example](../../docs/specification/21-worked-example.md)
(ground truth for `atlas::RequestResult`/`atlas::accept`/`atlas::reject`'s exact shape and call sites).

## Open questions for review

- `reject()` stores whatever `std::string` it is given, including an empty one, without an invariant check.
  Should an empty rejection reason instead be disallowed (e.g. via a precondition or a `std::optional`-returning
  factory), given §6 frames rejection as always needing "a capability-defined precondition" behind it?
- `describe()` is a small step toward the §6 uniform runtime failure channel without implementing any of the
  channel's actual delivery mechanism (event publication, subscription). Confirm this is the right amount to
  build now versus waiting entirely for `atlas-runtime`/`atlas-replication` to exist and shaping this rendering
  logic around their actual needs.
- `accept()`/`reject()`'s `RequestContract<T>` constraint is not independently re-verified by a compile-fail
  test in this library's own suite (a `requires`-expression checking non-`RequestContract` types are rejected
  hits a GCC 13 libstdc++ concepts diagnostic-in-immediate-context limitation — see the comment in
  `tests/atlas-request/request_result_test.cpp`). Coverage instead relies on `atlas-contracts`' own test suite
  proving `RequestContract<T>` itself rejects such types. Flagging in case a reviewer wants that constraint
  re-verified from this library's side once the GCC/libstdc++ combination in CI allows it.

## Dependency position

`atlas-request` depends on `atlas-contracts` (for `atlas::RequestContract<T>`, which constrains
`accept()`/`reject()`) plus `atlas_project_options`/`atlas_project_warnings` and the standard library. Per §5,
this sits at capability-facing infrastructure built directly on generated contracts, depending only downward;
it introduces no dependency on `atlas-entity`, `atlas-runtime`, or any other library beyond what the test suite
uses to reproduce §21's `ApplyDamage` verbatim (`atlas::entity`, test-only, mirroring `atlas-contracts`' own
test dependency).
