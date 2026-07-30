#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace atlas::cgen {

// A host manifest (spec §14, Declarative Source Format): which capabilities
// a host composes, by name - `host: GameplayClient` / `composes: [entity,
// health, ...]`, the exact shape §14/§21 show. Unlike a capability manifest
// (Manifest, manifest.hpp), a host declares no properties/requests/events
// of its own - it only names which already-declared capabilities are part
// of it, feeding the same dependency graph and cycle rules any composition
// is checked against (§5; see resolve_host_composition in
// host_composition.hpp).
struct HostManifest {
    std::string host_name;
    std::vector<std::string> composes;
};

// Parses host manifest YAML text into a HostManifest. Throws
// std::invalid_argument for malformed YAML, a missing/non-scalar `host:`
// key, or a `composes:` key present but not a sequence - a missing
// `composes:` block is not an error, and parses as an empty list (a host
// composing nothing yet is a degenerate case for resolve_host_composition
// to reject or accept, not a parse error).
[[nodiscard]] HostManifest parse_host_manifest(std::string_view yaml_text);

} // namespace atlas::cgen
