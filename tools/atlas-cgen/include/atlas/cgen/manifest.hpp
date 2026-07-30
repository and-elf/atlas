#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace atlas::cgen {

// A single named field within a property/request/event struct, e.g.
// "current: int32" inside the health capability's Health property (spec §21).
struct Field {
    std::string name;
    std::string type; // raw manifest type token, e.g. "int32", "EntityRef"
};

// One property, request, or event declaration - e.g. `Health`, `ApplyDamage`,
// `HealthChanged` in the §21 worked example. The manifest doesn't distinguish
// their shape (all three are just name + ordered fields); which YAML block
// (properties/requests/events) it came from is what decides the emitted
// concept (PropertyContract/RequestContract/EventContract).
struct StructDecl {
    std::string name;
    std::vector<Field> fields;

    // The raw manifest composition-strategy token (e.g. "Additive"), for a
    // property declared with a `composition:` key (spec §20) - std::nullopt
    // for a non-composed property, and always std::nullopt for a request or
    // event (composition is a property-only concept; see parse_manifest,
    // which only recognizes this key while parsing the properties block).
    // Stores the raw token rather than the mapped C++ spelling, mirroring
    // how Field::type stores the raw manifest type token and leaves mapping
    // to map_field_type() at render time - see map_composition_strategy().
    std::optional<std::string> composition;
};

// The subset of a capability manifest (spec §13, Capability Manifest) this
// generator round supports: capability.name, depends_on, consumes,
// properties, requests, events. A real manifest also carries
// version/source/contracts - those are deliberately not parsed yet (see
// README) and are ignored rather than rejected, for forward compatibility
// with real manifests.
struct Manifest {
    std::string capability_name;
    std::vector<std::string> depends_on;

    // Property names this capability reads but does not itself declare under
    // `properties:` (spec §5-adjacent: "systems coupled to data, not
    // implementation" - see property_graph.hpp, which resolves each entry
    // here to whichever composed capability's own `properties:` block
    // declares it, rather than this manifest naming that capability
    // directly). Additive to depends_on, not a replacement for it: a
    // dependency that isn't property flow (a direct function call, a shared
    // event type, a vocabulary type like ResourceId) still belongs in
    // depends_on.
    std::vector<std::string> consumes;

    std::vector<StructDecl> properties;
    std::vector<StructDecl> requests;
    std::vector<StructDecl> events;
};

// Parses manifest YAML text into a Manifest. Throws std::invalid_argument,
// with a human-readable message describing exactly what was wrong
// (malformed YAML, missing capability.name, or a field type outside the
// small closed set this generator maps to C++ - see map_field_type), rather
// than guessing at intent for anything it can't confidently parse.
//
// NOTE: this project generally prefers std::expected for fallible operations
// (CLAUDE.md) - but libstdc++'s <expected> gates its entire contents behind
// `__cpp_concepts >= 202002L`, and Clang (tested: 18) reports 201907L, so
// <expected> is simply unavailable when compiling this tool with Clang
// against libstdc++ (this project's exact CI configuration, not a
// hypothetical). Exceptions are used here instead - already how this file
// interacts with yaml-cpp's own YAML::Exception - rather than either
// breaking the Clang build or hand-rolling a parallel "expected but it
// actually works" type for one tool.
[[nodiscard]] Manifest parse_manifest(std::string_view yaml_text);

// Maps a manifest type token (e.g. "int32", "EntityRef") to its C++
// spelling (e.g. "std::int32_t", "atlas::EntityRef"), or std::nullopt if the
// token isn't in this generator's small, closed, deliberately-not-inferred
// set of supported types. Shared by parse_manifest (to validate a manifest's
// declared types up front) and contract_writer (to actually emit them).
//
// Vocabulary types (EntityRef, ResourceId) that a field can be declared with
// belong here - not "resource compilation" or any other tool's job. A
// manifest field being typed as a resource identity is an ordinary
// structural fact the generator needs to know how to emit, the same as
// EntityRef; it's a different question from whether some other tool (e.g. a
// future resource compiler) turns authored asset lists into compiled
// ResourceId tables in the first place (spec §12 lists "contract
// generation" and "resource compilation" as separate Atlas Tooling
// responsibilities).
[[nodiscard]] std::optional<std::string> map_field_type(const std::string& yaml_type);

// The header a generated contract must #include for a given manifest type
// token to compile, or std::nullopt if none is needed (primitives are
// covered by the contract file template's own unconditional includes).
// contract_writer collects the union of these across every field actually
// used, rather than a single hard-coded "does this manifest use EntityRef"
// check - see contract_writer.cpp.
[[nodiscard]] std::optional<std::string> required_include_for_type(const std::string& yaml_type);

// Maps a manifest composition-strategy token (e.g. "Additive") to its C++
// atlas::Composition enumerator spelling (e.g. "atlas::Composition::Additive"),
// or std::nullopt if the token isn't one of §20's fixed seven strategy
// names. Case-sensitive, matching the enumerator spelling exactly - "Additive"
// declared exhaustively even though atlas-runtime's composition engine only
// has a working evaluator for Additive so far, the same reasoning
// atlas::Composition itself documents (a strategy name is part of a
// property's compile-time contract regardless of which strategies have an
// evaluator yet).
[[nodiscard]] std::optional<std::string> map_composition_strategy(const std::string& yaml_composition);

} // namespace atlas::cgen
