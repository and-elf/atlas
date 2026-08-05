#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace atlas::rcc {

// Optional animation-specific resolution data for a resource entry, e.g.
//   animation:
//     skeleton: characters/hero/skeleton
//     loop: true
//     playback_rate: 1.0
// Deliberately parsed whenever an `animation:` block is present on an entry,
// regardless of what string that entry's `type` field holds (issue #45) -
// `type` is open-ended, capability-authored vocabulary this tool has no
// closed list to validate against (see README's "Real discoveries"), so
// gating this block's parsing on the literal string "Animation" would
// invent exactly the kind of closed-vocabulary special case that stance
// rejects. `skeleton` stays a raw resource *name* string here, same as
// ResourceEntry::name itself - resolving it to an atlas::ResourceId is
// compile_resource_table's job (resource_table.hpp), not parsing's.
struct AnimationMetadata {
    std::string skeleton;
    bool loop = false;
    double playback_rate = 1.0;
};

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
    std::optional<AnimationMetadata> animation;
};

// Parses resource manifest YAML text (a `resources:` list of
// name/type/path entries, each optionally carrying an `animation:` block -
// see AnimationMetadata) into an ordered list of ResourceEntry, preserving
// authored order. Throws std::invalid_argument, with a human-readable
// message describing exactly what was wrong (malformed YAML, a non-mapping
// root, a missing/malformed `resources:` list, an entry missing a required
// field or carrying an unrecognized one, an empty field value, a
// duplicate resource name, or a malformed `animation:` block - a
// non-mapping value, a missing/empty `skeleton`, a non-boolean `loop`, a
// non-numeric or non-positive `playback_rate`, or an unrecognized field
// inside it), rather than guessing at intent for anything it can't
// confidently parse - mirroring atlas-cgen's parse_manifest.
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
