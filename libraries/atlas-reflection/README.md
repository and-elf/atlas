# atlas-reflection

**Status:** Seeded. Implements `atlas::reflection::field_count<T>()`
(`include/atlas/reflection/field_count.hpp`) — a compile-time (`consteval`) primitive that counts the direct
data members of an aggregate `T`, using the well-known "how many braces does it take" technique: probe
increasing counts of an any-convertible placeholder type through aggregate initialization until one more
element stops compiling — plus `atlas::reflection::Reflectable<T>`, the concept constraining which types
`field_count()` accepts, and `atlas::reflection::describe_field_count()`
(`include/atlas/reflection/field_summary.hpp`) — a small runtime helper that renders a field count for
tooling/log display ("2 fields", "1 field"). Nothing else in this library's eventual scope (richer structural
metadata — field names, field types, per-field composition-strategy reflection for §20 — or any actual
generator/tooling integration) is implemented yet; that depends on the not-yet-built manifest-to-C++ generator
this task was explicit is out of scope.

## Scoping decision: why the brace-counting technique, not a narrower fallback

The assignment brief for this library flagged that the brace-counting technique has known cross-compiler edge
cases (aggregates with base classes, bit-fields, reference members) and offered a narrower, more conservative
fallback (just a bool answering "is this default-constructible and an aggregate") if the technique proved too
fragile to trust.

Rather than assume fragility, this implementation empirically probed each cited edge case (see
`tests/atlas-reflection/field_count_test.cpp`, "Known limitations" section) against both GCC 13 and Clang 18
before deciding:

- **Every real contract struct in this repository today** — `atlas::EntityRef`, `atlas::contracts::
  ContractVersion`, and the `Health`/`ApplyDamage`/`HealthChanged` shapes §21's worked example generates — is a
  flat aggregate of scalars and/or other flat class-typed members (like `EntityRef` itself used inside
  `ApplyDamage`/`HealthChanged`). The brace-counting technique counts every one of these **correctly**, verified
  directly against `atlas::EntityRef` and against local reproductions of the other three (mirroring how
  `atlas-contracts`'s own tests ground themselves in the same §21 shapes).
- **Bit-fields**, specifically called out as a caution, turned out **not** to be a problem for this technique —
  it never takes a bit-field's address or binds a reference to one (unlike structured-binding-based reflection,
  where that genuinely would fail). Verified with a dedicated test rather than assumed.
- **Base classes** are a real caveat, but not the naive one: a base-class subobject is a class type, so the
  placeholder type converts to it directly as a single opaque unit — the base reads as *exactly one field*
  regardless of how many members it itself declares. Neither "excluded" nor "flattened," which is worth knowing
  before trusting a count on a type with inheritance.
- **C-style array members** are a caveat this task's brief didn't call out, but testing found one anyway: a
  conversion-operator template cannot be instantiated to return an array type, so the compiler brace-elides
  through an array member and the search reports its *flattened element count* instead of treating the array as
  one field.
- **Reference members** without a default member initializer are excluded before `field_count()` is even
  reachable: such a type isn't default-constructible (a reference can't be left unbound), which the
  `Reflectable` concept already requires. A reference member *with* a default member initializer would still
  slip through uncounted correctly; that residual case is out of scope (documented, not guarded against) since
  no capability-declared contract in Atlas has reason to hold raw C++ reference members — properties and
  requests carry values or `EntityRef`s, never references, by the Rule-of-Zero convention this codebase already
  follows.

Given the caveats are real but confined to shapes no current or realistically-near-future contract struct uses,
and are pinned by tests rather than left as unverified prose, the technique earns its keep over the narrower
fallback: it answers a genuinely new question ("how many fields does this contract have") that the fallback
(a boolean) cannot.

## Provides

Runtime metadata access (`field_count()`, evaluated at compile time but callable from ordinary code), reflected
structure discovery (walking a contract struct's shape without per-type code), tooling integration groundwork
(`describe_field_count()`, a generic display primitive with no knowledge of which contract it's describing).

## Spec

[§3 Architectural Definitions](../../docs/specification/03-architectural-definitions.md) (Tooling — "generates
reflection metadata"), [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities)
(responsibility), [§18 Editor Extensions](../../docs/specification/18-editor-extensions.md) ("generic editing
through generated reflection metadata" — the vision this primitive is groundwork for), [§20 Property and
Resource Composition](../../docs/specification/20-property-and-resource-composition.md#tooling-support) (Tooling
Support — "tooling can display a property's full derivation without any property-specific tooling code"),
[§21 Worked Example](../../docs/specification/21-worked-example.md) (ground truth `Health`/`ApplyDamage`/
`HealthChanged` shapes this library's tests reproduce).

## Dependency position

`atlas-reflection` depends only on `atlas_project_options`/`atlas_project_warnings` and the standard library —
no dependency on `atlas-contracts`, deliberately: per §5, contracts sit *above* runtime libraries in the
dependency chain (Capability → Contracts → Runtime → Platform), so a runtime library depending on it would
invert that direction (see `field_count.hpp`'s comment on `Reflectable` for the same reasoning inline). The test
suite additionally links `atlas::entity` (test-only, not a library dependency), following the same precedent
`atlas-contracts`'s test suite already established, so `field_count_test.cpp` can verify `field_count()` against
the real `atlas::EntityRef` vocabulary type itself, not just a local reproduction of its shape.
