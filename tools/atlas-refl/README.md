# atlas-refl

All Atlas platform tools are prefixed `atlas-` (library naming convention extended to tools), with the tool's C++ namespace matching the suffix (`atlas::refl`, mirroring `atlas::cgen`/`atlas::rcc`/`atlas::core`).

## Why "atlas-refl"

Spec §12 (Build Model) lists reflection metadata generation as one of five distinct Atlas Tooling responsibilities, separate from contract generation (`atlas-cgen`), resource compilation (`atlas-rcc`), dependency-graph validation, and documentation generation. Following the project's existing short-suffix convention (`cgen` for contract *gen*eration, `rcc` for *r*esource *c*ompilation *c*ompiler) rather than a longer, more literal name, `refl` reads unambiguously as "reflection" while staying the same length class as its siblings and matching the exact name CLAUDE.md itself suggests as an example (`e.g. atlas-refl`) for this issue. `atlas::refl` is the C++ namespace, mirroring `atlas::cgen`/`atlas::rcc`.

**Status:** Implemented for a first, deliberately narrow slice: given a capability manifest YAML (the same `capability.name` / `properties` / `requests` / `events` shape `atlas-cgen` parses — spec §21's worked example), emits a small generated header of reflection metadata: one `constexpr std::array<atlas::refl::FieldMetadata, N>` per property/request/event struct, holding each field's declared name and the C++ spelling of its declared type, plus a `constexpr std::string_view` composition-strategy constant for a property that declares a `composition:` key (spec §20). Verified against the real `health` capability from §21 two ways: a unit test asserting on the generated text (`tests/atlas-refl/reflection_writer_test.cpp`), and a build-time target (`tests/atlas-refl/compile_check/`) that runs both the real `atlas-cgen` and `atlas-refl` binaries against the same fixture manifest and cross-checks their output against each other and against `atlas-reflection`'s own runtime primitives — a genuine compile-time proof, not a simulated one.

**Provides:** manifest parsing (a reflection-metadata-scoped subset of `atlas-cgen`'s manifest vocabulary), reflection metadata generation (spec §12's "Atlas Tooling" — reflection metadata generation).

**Spec:** [§12 Build Model](../../docs/specification/12-build-model.md) (reflection metadata generation as a distinct Atlas Tooling responsibility), [§13 Library Architecture — Capability Manifest](../../docs/specification/13-library-architecture.md), [§18 Editor Extensions](../../docs/specification/18-editor-extensions.md) ("generic editing through generated reflection metadata" — the vision this closes the missing half of), [§20 Property and Resource Composition](../../docs/specification/20-property-and-resource-composition.md#tooling-support) (Tooling Support — "tooling can display a property's full derivation without any property-specific tooling code"), [§21 Worked Example](../../docs/specification/21-worked-example.md) (ground truth `Health`/`ApplyDamage`/`HealthChanged` shapes this tool's tests reproduce).

## What this closes: the gap `atlas-reflection`'s own README names

`libraries/atlas-reflection` (the runtime library this tool's output is meant to complement, read in full before working on this tool) already provides `field_count<T>()`, `for_each_field(obj, visitor)`, and `field_types_t<T>` — genuine, standard C++23 primitives that can count a contract struct's direct data members and recover each one's real *type*. Its own README is explicit about what it *cannot* do in standard C++23: recover a field's *name* as a string (that's C++26 P2996 static reflection, with no C++23 equivalent short of macros or compiler-specific builtins this project's own conventions rule out), and it names this as deferred to "a future reflection-metadata-generation tool" rather than faked.

That is exactly this tool's job. A capability manifest already has each field's name as an ordinary YAML key — a fact this generator can read at generation time, when the compiled struct itself no longer can. `atlas-refl` emits that name (plus the type spelling, plus a property's composition strategy) as generated data alongside the generated contract, so a caller wanting both "how many fields, what are their real types" (from `atlas-reflection`, walking the real struct) and "what is this field called" (from `atlas-refl`'s generated array, indexed the same way) has both halves without inventing anything the standard forbids. `tests/atlas-refl/compile_check/health_reflection_compile_check.cpp` proves this pairing for real: it `static_assert`s that `atlas::reflection::field_count<atlas::health::Health>()` and `atlas::refl::health::kHealthFields.size()` agree, using both tools' actual generated output for the same fixture, not a hand-copied stand-in for either.

## Scope

Deliberately narrow, matching this project's established discipline of shipping one minimal real slice rather than the eventual full scope (the same discipline `tools/atlas-cgen/README.md` and `tools/atlas-rcc/README.md` document for their own first rounds):

- **Parses:** `capability.name`, `properties`/`requests`/`events` (each a map of struct name → field name → type), and a property's optional `composition:` key (spec §20). Unknown top-level keys (`depends_on`, `consumes`, `version`, `source`, `contracts`, a property's `trigger:` key) are ignored, not rejected, for forward compatibility with real manifests and with `atlas-cgen`'s own richer parsed shape — none of them affect a struct's *reflected* shape, which is all this generator emits. Widening this to cover `trigger:` (spec §20, Triggered composition) is a natural, small follow-up once a real consumer wants it reflected too; deliberately not spending this round's scope on it since no test today needs it.
- **Type mapping** is the same small, closed table `atlas-cgen` maps (`int8`–`uint64`, `float`, `double`, `bool`, `EntityRef`, `ResourceId`) — anything else is a hard parse error, never guessed at. The emitted `type_name` is the *mapped* C++ spelling (e.g. `"std::int32_t"`, `"atlas::EntityRef"`), matching what the real generated contract struct's field type actually is, not the raw manifest token — so a consumer reading `FieldMetadata::type_name` sees the same spelling the field is actually declared with.
- **Composition strategy** is emitted as the mapped `atlas::Composition` enumerator spelling (e.g. `"atlas::Composition::Additive"`, matching `atlas-cgen`'s own `static constexpr auto composition = ...;` member exactly), as a plain descriptive `std::string_view` constant — not a typed reference to the real `atlas::Composition` enum, since this generator has no dependency on `atlas-contracts`/`atlas-runtime` (see "Dependency position" below) and the metadata is meant for generic tooling display (spec §20, Tooling Support), not for driving the composition engine itself.
- **Does not** validate `depends_on`/`consumes` (an ordering concern — `atlas-cgen`'s job, spec §5), does not process `source:`/`contracts:` blocks, does not emit anything for a property's `trigger:` key, and does not have a host-composition mode (`atlas-cgen --host`'s counterpart) aggregating reflection metadata across a whole composed host — this round mirrors `atlas-cgen`'s own original single-capability-mode scope, not its later host-composition round.
- **`main.cpp` (the CLI)** is excluded from the coverage gate (`cmake/CodeCoverage.cmake`'s existing `tools/[^/]+/src/main.cpp` glob already covers it without modification) — its argv/file-I/O error paths need subprocess-spawning test infrastructure this project doesn't build yet, the same reasoning `tools/atlas-cgen/README.md` and `tools/atlas-rcc/README.md` both give for their own CLIs. The library logic it wires together (`parse_manifest`, `generate_reflection_metadata`, `render_template`) is fully unit-tested in `tests/atlas-refl/`.

## Generated output shape

For `tests/fixtures/health.capability.yaml` (spec §21's worked example), `atlas-refl health.capability.yaml health.capability.reflection.hpp` emits, in outline:

```cpp
// GENERATED — health.capability.reflection.hpp
// Source: health.capability.yaml — do not hand-edit.
#pragma once
#include "atlas/refl/field_metadata.hpp"
#include <array>
#include <string_view>

namespace atlas::refl::health {

inline constexpr std::array<atlas::refl::FieldMetadata, 2> kHealthFields{ {
    {"current", "std::int32_t"},
    {"maximum", "std::int32_t"},
} };

inline constexpr std::array<atlas::refl::FieldMetadata, 2> kApplyDamageFields{ {
    {"target", "atlas::EntityRef"},
    {"amount", "std::int32_t"},
} };

inline constexpr std::array<atlas::refl::FieldMetadata, 2> kHealthChangedFields{ {
    {"target", "atlas::EntityRef"},
    {"new_current", "std::int32_t"},
} };

}  // namespace atlas::refl::health
```

One array per struct, named `k<StructName>Fields`, in manifest declaration order (properties, then requests, then events — matching `atlas-cgen`'s own `generate_contract` emission order exactly, so the two generators' output stays predictably ordered relative to each other). A composed property (e.g. `tests/fixtures/armor.capability.yaml`'s `Armor`) additionally gets `inline constexpr std::string_view kArmorComposition = "atlas::Composition::Additive";`, appearing after its fields array.

Field entries use the well-known "double-brace" `std::array` idiom (`kHealthFields{ {...} }`, not `kHealthFields{...}`) rather than relying on brace elision across an aggregate element type — this was a real discovery made building this tool (see below), not an arbitrary style choice.

## Real discoveries made building this

- **`std::array<Aggregate, N>{ {a}, {b} }` needs the extra brace level; `std::array<Aggregate, N>{a, b}` does not compile.** The first version of `struct_metadata.tmpl`'s per-field initializer list omitted the extra outer brace pair around the fields (relying on `std::array`'s well-known single-level brace elision the same way `std::array<int, 3> a{1, 2, 3};` works). GCC 13 rejected it with "too many initializers for 'const std::array<atlas::refl::FieldMetadata, 2>'": `std::array<T, N>` has exactly one direct member (the raw `T[N]` array), and once an initializer clause is itself already a braced-init-list (`{"current", "std::int32_t"}`, needed here since `FieldMetadata` is itself an aggregate of two fields), brace elision does not additionally flatten multiple such clauses across the *outer* aggregate boundary the way it does for scalar element types. The fix — confirmed by rebuilding and rerunning `tests/atlas-refl/compile_check/`'s build-time proof — is the same extra-brace idiom commonly needed for `std::array<std::pair<...>, N>` and similar: `std::array<T, N>{ {a}, {b} };`.
- **Reusing `cmake/scripts/EmbedTextFile.cmake` verbatim would have been wrong, not just untidy.** That script hard-codes the namespace it writes embedded template constants into as `atlas::cgen::templates`, regardless of the `CPP_NAME`/output path given to it — reasonable for a script that, so far, only ever had one caller. Since this round's change footprint is deliberately kept strictly inside `tools/atlas-refl/`/`tests/atlas-refl/` (no `cmake/` edits — see "Dependency position"), reusing it as-is would have put this tool's own template constants under the wrong tool's namespace. This tool's two small template strings (`reflection_file_template`, `struct_metadata_template`) are instead hand-written `constexpr std::string_view` constants directly in `src/reflection_writer.cpp`, rather than separate `.tmpl` files embedded via that script. `render_template`'s generic `{{PLACEHOLDER}}` substitution engine (`template_engine.hpp`/`.cpp`) is still exactly `atlas-cgen`'s own engine, structurally duplicated — so the "template text plus C++ code assembling repeated/conditional content" split (this project's Declarative Boundary principle, CLAUDE.md / spec §14) is identical either way; only the delivery mechanism for the two small template strings differs. Generalizing `EmbedTextFile.cmake` to take an explicit namespace argument, so a future round of this tool (or any other) could use file-based `templates/*.tmpl` the way `atlas-cgen` does, is a natural, separate follow-up.

## Dependency position

Depends on `yaml-cpp` (fetched, `SYSTEM`) for manifest parsing — duplicated from `tools/atlas-cgen/CMakeLists.txt` verbatim (same `GIT_TAG 0.8.0`), not shared via a common CMake include. `FetchContent_Declare`/`_MakeAvailable` are idempotent per dependency name, so declaring `yaml-cpp` a second time across sibling `tools/` subdirectories does not re-download or conflict — the same precedent `tools/atlas-rcc/README.md` documents for its own duplicated declaration. Does not depend on any other `atlas-*` library or tool: `atlas::Composition`, `atlas::PropertyContract`, etc. are referenced only as *emitted text* (a `type_name`/composition-constant string), never linked against, the same as `atlas-cgen`'s own relationship to those vocabulary types. Does not depend on `atlas-cgen` (or vice versa) — the two tools parse an overlapping but independently-implemented subset of the same manifest vocabulary (`Field`/`StructDecl`/`Manifest` in this tool's `manifest.hpp` deliberately mirror `atlas::cgen`'s own shapes rather than reusing them), matching `tools/atlas-cgen/README.md`'s own "Contract generation vs. resource compilation" precedent: one tool generates contract *shape*, the other generates reflection *metadata about* that shape — genuinely separate compile-time responsibilities (spec §12), not two modes of one tool, and neither should gain a build-ordering dependency on the other's `CMakeLists.txt` existing or running first.

`tests/atlas-refl/compile_check/` (not the library itself) links `atlas::contracts`, `atlas::entity`, and `atlas::reflection` — and, to reach `atlas::refl::FieldMetadata`'s definition, `atlas::refl_lib` — to prove generated output from *both* `atlas-cgen` and `atlas-refl` actually compiles together and satisfies `atlas-reflection`'s real primitives.

## Reflection metadata generation vs. contract generation vs. resource compilation

Spec §12 lists these as three separate Atlas Tooling responsibilities. `atlas-cgen` emits a capability's contract *shape* (the actual C++ struct, with `static_assert`s against `atlas-contracts`' concepts) — a manifest field typed `ResourceId` is, from its point of view, a vocabulary type it must know how to emit. `atlas-rcc` compiles *resource data* (an authored asset list into `atlas::ResourceId`-keyed tables) — a fundamentally different question from either struct shape or field metadata. `atlas-refl`, this tool, emits *metadata describing* an already-generated struct's shape (field names, field type spellings, composition strategy) for generic tooling to walk without per-type hand-written reflection code — it never emits the struct itself, and never compiles resource data. All three read overlapping manifest vocabulary; none of the three depends on either of the others.
