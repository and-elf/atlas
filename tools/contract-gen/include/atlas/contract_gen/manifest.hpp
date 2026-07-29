#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace atlas::contract_gen {

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
};

// The subset of a capability manifest (spec §13, Capability Manifest) this
// generator round supports: capability.name, depends_on, properties,
// requests, events. A real manifest also carries version/source/contracts -
// those are deliberately not parsed yet (see README) and are ignored rather
// than rejected, for forward compatibility with real manifests.
struct Manifest {
    std::string capability_name;
    std::vector<std::string> depends_on;
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
[[nodiscard]] std::optional<std::string> map_field_type(const std::string& yaml_type);

} // namespace atlas::contract_gen
