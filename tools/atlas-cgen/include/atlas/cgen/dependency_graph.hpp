#pragma once

#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace atlas::cgen {

// One node in a capability dependency graph: a name (a capability's own,
// spec §13) and the names it declares via `depends_on`. Deliberately
// smaller than a full Manifest (properties/requests/events are irrelevant
// to graph resolution itself) - callers building a graph from real
// manifests project down to this shape first (see host_composition.hpp).
struct DependencyNode {
    std::string name;
    std::vector<std::string> depends_on;
};

// A capability composition order (spec §5, Ordering Without Stages):
// names in dependency order - every name's own `depends_on` entries appear
// earlier in the list than the name itself. Execution/initialization
// ordering (e.g. "presentation runs after simulation") falls out of this
// order alone, never a separate declared stage/tier/phase.
using CompositionOrder = std::vector<std::string>;

// A dependency cycle (spec §5): "an invalid composition and a hard
// compile-time build failure" - what() names the full chain of edges
// forming the cycle, in order (e.g. "dependency cycle: a -> b -> c -> a"),
// not just the first offending dependency, per §5's explicit requirement.
class DependencyCycleError : public std::invalid_argument {
public:
    explicit DependencyCycleError(const std::string& message) : std::invalid_argument(message) {}
};

// Resolves `nodes` into a topological composition order via depth-first
// search: dependencies always precede their dependents, and a diamond
// dependency (two nodes sharing a common dependency) still places the
// shared node exactly once. Traversal order is deterministic (spec §4) -
// nodes are visited in the order given, and each node's own depends_on list
// is walked in the order given - never driven by hash-map iteration.
//
// Throws std::invalid_argument if any node names a dependency that isn't
// itself present among `nodes` (an unresolved dependency - a setup
// mistake, not a cycle) or if `nodes` contains two entries with the same
// name. Throws DependencyCycleError (a std::invalid_argument) if the graph
// contains a cycle, including a self-dependency.
[[nodiscard]] CompositionOrder resolve_composition_order(std::span<const DependencyNode> nodes);

} // namespace atlas::cgen
