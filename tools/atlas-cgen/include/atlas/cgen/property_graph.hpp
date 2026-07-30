#pragma once

#include "atlas/cgen/manifest.hpp"

#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace atlas::cgen {

// Thrown when more than one manifest declares the same property name under
// its own `properties:` block (issue #16 - the design discussion's
// checklist item "exactly one provider for non-aggregated properties").
// Property aggregation (multiple *contributors* to one property's value,
// merged by a declared `composition:` strategy - spec §20) is a separate,
// already-solved concern from this: this error is about which single
// capability is responsible for computing a property in the first place.
class PropertyProviderConflictError : public std::invalid_argument {
public:
    explicit PropertyProviderConflictError(const std::string& message) : std::invalid_argument(message) {}
};

// Maps each property name declared under some manifest's `properties:`
// block, across every manifest in `manifests`, to that manifest's own
// capability_name. Throws PropertyProviderConflictError, naming both
// offending capabilities, if two manifests in `manifests` declare the same
// property name - callers decide which manifests to pass (e.g. resolve_host_
// composition scopes this to the composed set only, mirroring how
// depends_on closure validation is already scoped).
[[nodiscard]] std::unordered_map<std::string, std::string>
resolve_property_providers(std::span<const Manifest> manifests);

// Thrown when a manifest's `consumes:` entry names a property nothing among
// the manifests `providers` was built from actually provides.
class UnresolvedPropertyConsumerError : public std::invalid_argument {
public:
    explicit UnresolvedPropertyConsumerError(const std::string& message) : std::invalid_argument(message) {}
};

// For every manifest in `composed`, resolves its `consumes:` entries against
// `providers` (as returned by resolve_property_providers) and returns, keyed
// by capability name, the provider capability names it should transitively
// depend on for ordering purposes - the derived "Property -> System"
// dependency edges the design discussion behind issue #16 calls for, so a
// manifest names the property it needs rather than the capability that
// happens to provide it. A manifest consuming a property it provides itself
// produces no edge for that property (nothing to order it before). Throws
// UnresolvedPropertyConsumerError if a consumed property isn't in
// `providers`. A capability with no derived edges has no entry in the
// returned map.
[[nodiscard]] std::unordered_map<std::string, std::vector<std::string>>
resolve_property_dependency_edges(std::span<const Manifest> composed,
                                  const std::unordered_map<std::string, std::string>& providers);

} // namespace atlas::cgen
