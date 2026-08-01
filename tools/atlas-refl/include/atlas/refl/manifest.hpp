#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace atlas::refl {

// A single named field within a property/request/event struct, e.g.
// "current: int32" inside the health capability's Health property (spec
// §21). Deliberately the same shape as atlas::cgen::Field
// (tools/atlas-cgen/include/atlas/cgen/manifest.hpp) - both tools parse the
// same manifest vocabulary, one into a generated contract struct, the other
// into generated reflection metadata about that struct (spec §12 lists
// these as two distinct Atlas Tooling responsibilities, not two modes of
// one tool - see this tool's README, "Contract generation vs. reflection
// metadata generation"). Duplicated rather than shared: atlas-refl does not
// depend on atlas-cgen (or vice versa), the same independent-dependency-
// position precedent tools/atlas-rcc/README.md documents for its own
// duplicated yaml-cpp FetchContent_Declare.
struct Field {
    std::string name;
    std::string type; // raw manifest type token, e.g. "int32", "EntityRef"
};

// One property, request, or event declaration. Reflection metadata only
// needs a struct's name, its fields, and (for a property) an optional
// composition strategy - not depends_on/consumes (ordering/composition
// concerns, atlas-cgen's job) or trigger (out of scope for this round, see
// README).
struct StructDecl {
    std::string name;
    std::vector<Field> fields;

    // The raw manifest composition-strategy token (e.g. "Additive"), for a
    // property declared with a `composition:` key (spec §20) - std::nullopt
    // for a non-composed property, and always std::nullopt for a request or
    // event (composition is a property-only concept; see parse_manifest,
    // which only recognizes this key while parsing the properties block).
    std::optional<std::string> composition;
};

// The subset of a capability manifest (spec §13, Capability Manifest) this
// generator needs: capability.name plus properties/requests/events. A real
// manifest also carries depends_on/consumes/version/source/contracts -
// those don't affect a struct's own reflected shape, so they are ignored
// here (not rejected, for forward compatibility with real manifests, the
// same convention atlas-cgen's parse_manifest already established).
struct Manifest {
    std::string capability_name;

    std::vector<StructDecl> properties;
    std::vector<StructDecl> requests;
    std::vector<StructDecl> events;
};

// Parses manifest YAML text into a Manifest. Throws std::invalid_argument,
// with a human-readable message describing exactly what was wrong
// (malformed YAML, missing capability.name, or a field type outside the
// small closed set this generator maps to C++ - see map_field_type), rather
// than guessing at intent for anything it can't confidently parse. See
// tools/atlas-cgen/include/atlas/cgen/manifest.hpp's own parse_manifest
// comment for why this throws rather than returning std::expected (Clang
// 18 + libstdc++'s <expected> is unavailable - not a hypothetical for this
// project's own CI toolchain).
[[nodiscard]] Manifest parse_manifest(std::string_view yaml_text);

// Maps a manifest type token (e.g. "int32", "EntityRef") to its C++
// spelling (e.g. "std::int32_t", "atlas::EntityRef"), or std::nullopt if the
// token isn't in this generator's small, closed, deliberately-not-inferred
// set of supported types - the exact same closed set atlas-cgen maps,
// duplicated for the same independent-dependency-position reason Field/
// StructDecl above are.
[[nodiscard]] std::optional<std::string> map_field_type(const std::string& yaml_type);

// Maps a manifest composition-strategy token (e.g. "Additive") to its C++
// atlas::Composition enumerator spelling (e.g. "atlas::Composition::Additive"),
// or std::nullopt if the token isn't one of §20's fixed seven strategy
// names. Case-sensitive, matching the enumerator spelling exactly.
[[nodiscard]] std::optional<std::string> map_composition_strategy(const std::string& yaml_composition);

} // namespace atlas::refl
