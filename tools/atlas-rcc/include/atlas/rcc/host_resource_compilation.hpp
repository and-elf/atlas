#pragma once

#include "atlas/rcc/resource_manifest.hpp"

#include <string>
#include <vector>

namespace atlas::rcc {

// One already-parsed resource manifest's entries, tagged with a label
// (typically its source file path) used only for merge_resource_manifests'
// own error messages - never carried into a CompiledResource or a packed
// blob.
struct ResourceManifestSource {
    std::string label;
    std::vector<ResourceEntry> entries;
};

// Merges N already-parsed resource manifests (see parse_resource_manifest -
// each source's own within-manifest name uniqueness is already enforced
// there) into one flat, ordered entry list for a host-scoped compilation run
// (spec §3, Resource: "scoped to what the composing host actually
// references"; issue #62): sources concatenated in the order given, each
// source's own entries kept in their authored order - a plain, deterministic
// concatenation, never an unordered merge (spec §4: avoid unordered
// iteration anywhere it could affect output), so compile_resource_table/
// pack_resource_blob downstream produce the exact same bytes as if every
// entry had been authored in one combined manifest to begin with.
//
// Throws std::invalid_argument if the same resource `name` appears in two
// different sources, naming the resource and both sources' labels - the
// same duplicate-name correctness bug parse_resource_manifest already
// rejects within one manifest, extended across a host-scoped run's whole
// composed set.
[[nodiscard]] std::vector<ResourceEntry>
merge_resource_manifests(const std::vector<ResourceManifestSource>& sources);

} // namespace atlas::rcc
