#pragma once

#include <string>
#include <string_view>

namespace atlas::rcc {

// A resource-compilation host manifest: purely a stable host *name* used to
// label a host-scoped compilation run (see host_resource_compilation.hpp) -
// deliberately not atlas-cgen's HostManifest shape (host_manifest.hpp),
// which additionally carries a `composes:` capability-name list resolved
// against a capability dependency graph. atlas-rcc has no such graph to
// resolve against (issue #62): a host-scoped run's actual resource manifest
// set is given explicitly as CLI arguments (mirroring how atlas-cgen's own
// --host mode is handed explicit capability.yaml file paths, not just
// names, for its composed set), so a `composes:` list naming manifests here
// would just duplicate argv and was deliberately left out - see
// tools/atlas-rcc/README.md's "Host-scoped compilation" section for the
// full rationale, including why this makes `host_name` a thinner concept
// here than in atlas-cgen.
struct HostResourceManifest {
    std::string host_name;
};

// Parses a host resource manifest (`host: Name`) into a
// HostResourceManifest. Throws std::invalid_argument for malformed YAML, a
// non-mapping document root, or a missing/non-scalar `host:` key.
[[nodiscard]] HostResourceManifest parse_host_resource_manifest(std::string_view yaml_text);

} // namespace atlas::rcc
