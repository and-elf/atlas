# atlas-reflection

**Status:** Seeded, with richer structural metadata now implemented on top of `field_count()`. Implements
`atlas::reflection::field_count<T>()` (`include/atlas/reflection/field_count.hpp`) — a compile-time
(`consteval`) primitive that counts the direct data members of an aggregate `T`, using the well-known "how many
braces does it take" technique: probe increasing counts of an any-convertible placeholder type through
aggregate initialization until one more element stops compiling — plus `atlas::reflection::Reflectable<T>`, the
concept constraining which types `field_count()` accepts, and `atlas::reflection::describe_field_count()`
(`include/atlas/reflection/field_summary.hpp`) — a small runtime helper that renders a field count for
tooling/log display ("2 fields", "1 field").

On top of that, `include/atlas/reflection/field_visitor.hpp` adds `atlas::reflection::for_each_field(obj,
visitor)` — visits each direct data member of `obj`, in declaration order, via a structured-bindings dispatch
table — and `atlas::reflection::field_types_t<T>`, a `std::tuple<...>` of `T`'s field types in declaration
order. Together these are the field-*type* and field-*visitation* structural metadata the previous Status line
named as the next step. Field *names* as strings remain out of reach in standard C++23 — see "Scoping decision:
field visitation and type inspection, not field names" below — and per-field composition-strategy reflection for
§20 remains deferred until the property composition system it depends on actually exists. No generator/tooling
integration is implemented yet either; that depends on the not-yet-built manifest-to-C++ generator work this
task was explicit is out of scope.

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

## Scoping decision: field visitation and type inspection, not field names

The brief for this round of work was explicit that real static reflection — recovering a field's *name* as a
string, e.g. `"current"` for `Health::current` — is a C++26 feature (P2996) with no standard C++23 equivalent,
and that faking it with macros or compiler builtins (`__builtin_dump_struct` and similar) would contradict this
repository's own stated preference for standard-library facilities over hand-rolled/compiler-specific ones. That
constraint was taken at face value rather than tested for loopholes: there is no standard C++23 mechanism that
recovers an identifier from a data member, full stop, so `for_each_field`/`field_types_t` do not attempt it, and
nothing in either header's implementation goes anywhere near a name.

What C++23 *does* give you, and what this library now builds on:

- **Structured bindings can decompose an aggregate into exactly as many named bindings as it has direct
  fields** — and `field_count<T>()` already tells you that number at compile time. `atlas::reflection::detail::
  tie_fields<T>()` (`field_visitor.hpp`) uses that number to select, via an `if constexpr` chain, one of a
  hand-written set of structured-binding declarations (`auto& [f0] = obj;`, `auto& [f0, f1] = obj;`, …) and
  returns `std::tie(f0, f1, ...)` — a tuple aliasing the real fields, not copies of them.
- **A generic (`auto&`) visitor recovers each field's real type** through `decltype` on its own parameter, in
  declaration order — `for_each_field(obj, [](auto& field) { using T = std::decay_t<decltype(field)>; ... })`.
  This is genuine, standard, per-field type inspection; it was verified against a heterogeneous contract shape
  (`ApplyDamage`, mixing an `atlas::EntityRef` field with a `std::int32_t` field) in
  `field_visitor_test.cpp`, not just against a homogeneous one where a type mismatch would be easy to miss.
- **`field_types_t<T>`** packages the same type information as a `std::tuple<...>` of decayed field types,
  for a caller that wants the shape without writing a visitor — built purely at the type level
  (`decltype(detail::tie_fields(std::declval<T&>()))`, an unevaluated `decltype` context), so it works for
  move-only field types too (verified against a local `MoveOnlyAggregate` reproduction, same as
  `field_count_test.cpp`'s).

**Why `std::tie`, not `std::make_tuple`, inside `tie_fields`:** copying every field out to build the tuple would
break for any move-only contract field (verified with `MoveOnlyAggregate`, which holds a `std::unique_ptr`) —
`std::tie` aliases the originals instead, so `for_each_field` can mutate through its visitor's `auto&` parameter
(verified in `ForEachField.VisitorCanMutateFieldsThroughTheAliasedReference`) and never needs the field type to
be copyable.

**Why the dispatch table caps at `max_supported_fields = 16`, not `field_count()`'s own 32-search cap:**
these are two different limits solving two different problems. `field_count()`'s `detail::max_searched_fields`
(32) only bounds a compile-time *search loop* — cheap to make generous. The cap in `field_visitor.hpp` bounds a
*hand-written* dispatch table, because a structured binding's identifier list (`auto& [f0, ..., fN-1] = obj;`)
cannot itself be produced by a template loop or a pack expansion — someone has to type out `f0` through `f15`
once per supported count. Every hand-written contract struct in this repository today (`atlas::EntityRef`,
`atlas::contracts::ContractVersion`, and §21's `Health`/`ApplyDamage`/`HealthChanged`) has one or two fields; 16
leaves an 8x margin over that before a shape is simply unsupported, in exchange for 16 textually-written cases
rather than an unbounded number. The cap is enforced as a **concept constraint** (`FieldVisitable`), not a bare
`static_assert` buried inside `tie_fields`'s dispatch chain — deliberately, because a constraint violation is a
SFINAE-friendly substitution failure a `requires`-expression can probe without aborting the whole translation
unit, whereas a hard `static_assert` failure inside an already-instantiated function body cannot. This is what
lets `field_visitor_test.cpp` actually test the boundary (`ForEachField.VisitsAllFieldsExactlyAtTheSupportedCap`
at exactly 16 fields, and `static_assert(!FieldVisitable<SeventeenFields>)` / `static_assert(!CanForEachField<
SeventeenFields>)` one field past it) as real, passing, compiled assertions rather than unverified prose about
what "should" happen past the cap.

**What's still out of scope, and why:** field *names* as strings (blocked on C++26 reflection, as above);
per-field composition-strategy reflection for §20 (Property and Resource Composition) — that depends on the
composition-strategy system itself, which doesn't exist in this repository yet, so building reflection for it
now would be speculating ahead of a not-yet-built dependency, not filling a gap; and any actual generator/tooling
integration (walking a *generated* contract's fields through these primitives from `atlas-cgen` or a future
reflection-metadata-generation tool) — this library provides the primitive, not the integration.

## Provides

Runtime metadata access (`field_count()`, evaluated at compile time but callable from ordinary code), reflected
structure discovery (`for_each_field()` walking a contract struct's fields in declaration order, and
`field_types_t<T>` exposing their types as a tuple, both without per-type hand-written code), tooling integration
groundwork (`describe_field_count()`, a generic display primitive with no knowledge of which contract it's
describing).

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
