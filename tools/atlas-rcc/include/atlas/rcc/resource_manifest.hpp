#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace atlas::rcc {

// A single authored resource declaration from a resource manifest, e.g.
//   - name: characters/hero/mesh
//     type: Mesh
//     path: characters/hero/mesh.fbx
// `name` is the resource's stable identity (spec §3, Resource: "resolved by
// stable identity, never a hard-coded path") - it is what
// atlas::ResourceId::from_name hashes, not `path`. `path` is carried through
// unchanged as the resolution data a downstream host needs to actually load
// the asset; this tool never interprets or validates it as a real filesystem
// path (existence, extension, etc. is out of scope - see README).
struct ResourceEntry {
    std::string name;
    std::string type;
    std::string path;
};

// Parses resource manifest YAML text (a `resources:` list of
// name/type/path entries) into an ordered list of ResourceEntry, preserving
// authored order. Throws std::invalid_argument, with a human-readable
// message describing exactly what was wrong (malformed YAML, a non-mapping
// root, a missing/malformed `resources:` list, an entry missing a required
// field or carrying an unrecognized one, an empty field value, or a
// duplicate resource name), rather than guessing at intent for anything it
// can't confidently parse - mirroring atlas-cgen's parse_manifest.
//
// NOTE: this project generally prefers std::expected for fallible operations
// (CLAUDE.md) - but libstdc++'s <expected> gates its entire contents behind
// `__cpp_concepts >= 202002L`, and Clang (tested: 18) reports 201907L, so
// <expected> is simply unavailable when compiling this tool with Clang
// against libstdc++ (this project's exact CI configuration, not a
// hypothetical). Exceptions are used here instead, consistent with
// atlas-cgen's manifest.hpp and this file's own interaction with yaml-cpp's
// YAML::Exception.
[[nodiscard]] std::vector<ResourceEntry> parse_resource_manifest(std::string_view yaml_text);

} // namespace atlas::rcc
