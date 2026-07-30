#pragma once

#include "atlas/cgen/host_manifest.hpp"
#include "atlas/cgen/manifest.hpp"

#include <span>
#include <string>
#include <vector>

namespace atlas::cgen {

// A host's fully resolved composition: its declared name, and every
// capability it composes, each with its complete Manifest (properties/
// requests/events, not just the name a HostManifest carries), in
// dependency order (spec §5) - the input generate_host_composition needs
// to emit the host's own PropertyStore registration (host_composition
// codegen lives alongside this in contract_writer.cpp/host_composition
// templates, not here - this file only resolves structure).
struct HostComposition {
    std::string host_name;
    std::vector<Manifest> ordered_capabilities;
};

// Resolves `host` against `available_manifests` (every capability manifest
// this generation run has been given - not necessarily all composed):
//
// - Every name in host.composes must resolve to a manifest actually present
//   in available_manifests, or this throws std::invalid_argument (composing
//   an unknown capability).
// - Every composed capability's own `depends_on` entries that *do* name a
//   known capability (present in available_manifests) must themselves be
//   composed too, or this throws std::invalid_argument - composition here
//   is explicit, never an implicit transitive include (spec §4: no
//   plugin discovery, nothing composed by magic). A depends_on entry with
//   no corresponding manifest at all (e.g. "entity", "resource" - a
//   foundational runtime-library dependency, never itself a capability
//   manifest) is silently treated as already satisfied.
// - The resulting graph (restricted to the composed set) is resolved via
//   resolve_composition_order (dependency_graph.hpp), which throws
//   DependencyCycleError on a cycle, reporting the full chain (spec §5).
// - available_manifests must not contain two manifests with the same
//   capability_name, or this throws std::invalid_argument.
[[nodiscard]] HostComposition resolve_host_composition(const HostManifest& host,
                                                       std::span<const Manifest> available_manifests);

} // namespace atlas::cgen
