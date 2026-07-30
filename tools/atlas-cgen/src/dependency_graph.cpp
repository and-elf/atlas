#include "atlas/cgen/dependency_graph.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace atlas::cgen {

namespace {

enum class VisitState : std::uint8_t { Visiting, Visited };

// Depth-first traversal state, gathered into one object purely so
// build_cycle_message can read `path` without threading it through every
// call - visit()'s own recursion is the graph traversal, this struct is
// not a class with an invariant to protect (Rule of Zero would apply if it
// had a constructor; it doesn't need one).
struct Resolver {
    // A pointer, not a reference (cppcoreguidelines-avoid-const-or-ref-data-members) -
    // functionally a non-owning back-reference to a lookup table owned by
    // resolve_composition_order, the same reasoning this repo's other
    // const-or-ref-data-member fixes already use (e.g. Context::host_,
    // atlas-cgen's own contract_writer.cpp Block).
    const std::unordered_map<std::string, const DependencyNode*>* lookup;
    std::unordered_map<std::string, VisitState> state;
    std::vector<std::string> path;
    CompositionOrder order;

    // path currently holds the DFS stack from some earlier node down to
    // closing_name's own dependent - closing_name itself is where the cycle
    // closes back to, so the chain is path[first occurrence of
    // closing_name..end] plus closing_name once more at the end (spec §5:
    // the *full* chain of edges, in order, not just the first offending
    // dependency).
    [[nodiscard]] std::string build_cycle_message(const std::string& closing_name) const {
        const auto start = std::find(path.begin(), path.end(), closing_name);
        std::string chain;
        for (auto it = start; it != path.end(); ++it) {
            if (!chain.empty()) {
                chain += " -> ";
            }
            chain += *it;
        }
        chain += " -> " + closing_name;
        return "dependency cycle: " + chain;
    }

    void visit(const std::string& name) {
        state[name] = VisitState::Visiting;
        path.push_back(name);

        // Guaranteed present: resolve_composition_order already validated
        // every depends_on entry resolves to a real node before any visit()
        // call happens.
        const auto& dependencies = lookup->at(name)->depends_on;
        for (const auto& dependency : dependencies) {
            const auto dependency_state = state.find(dependency);
            if (dependency_state == state.end()) {
                visit(dependency);
                continue;
            }
            if (dependency_state->second == VisitState::Visiting) {
                throw DependencyCycleError(build_cycle_message(dependency));
            }
            // Already Visited: reached via a different path (a diamond
            // dependency) - nothing left to do for it.
        }

        path.pop_back();
        state[name] = VisitState::Visited;
        order.push_back(name);
    }
};

} // namespace

CompositionOrder resolve_composition_order(std::span<const DependencyNode> nodes) {
    std::unordered_map<std::string, const DependencyNode*> lookup;
    for (const auto& node : nodes) {
        if (!lookup.emplace(node.name, &node).second) {
            throw std::invalid_argument("duplicate capability name '" + node.name + "' in dependency graph");
        }
    }

    for (const auto& node : nodes) {
        for (const auto& dependency : node.depends_on) {
            if (!lookup.contains(dependency)) {
                throw std::invalid_argument("capability '" + node.name + "' depends on unknown capability '" +
                                            dependency + "'");
            }
        }
    }

    Resolver resolver{.lookup = &lookup, .state = {}, .path = {}, .order = {}};
    for (const auto& node : nodes) {
        if (!resolver.state.contains(node.name)) {
            resolver.visit(node.name);
        }
    }

    return std::move(resolver.order);
}

} // namespace atlas::cgen
