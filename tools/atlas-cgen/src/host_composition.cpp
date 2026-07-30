#include "atlas/cgen/host_composition.hpp"

#include "atlas/cgen/dependency_graph.hpp"

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace atlas::cgen {

HostComposition resolve_host_composition(const HostManifest& host,
                                         std::span<const Manifest> available_manifests) {
    std::unordered_map<std::string, const Manifest*> available;
    for (const auto& manifest : available_manifests) {
        if (!available.emplace(manifest.capability_name, &manifest).second) {
            throw std::invalid_argument("duplicate capability manifest '" + manifest.capability_name + "'");
        }
    }

    std::vector<const Manifest*> composed;
    composed.reserve(host.composes.size());
    for (const auto& name : host.composes) {
        const auto it = available.find(name);
        if (it == available.end()) {
            throw std::invalid_argument("host '" + host.host_name + "' composes unknown capability '" + name +
                                        "'");
        }
        composed.push_back(it->second);
    }

    const std::unordered_set<std::string> composed_names(host.composes.begin(), host.composes.end());

    std::vector<DependencyNode> nodes;
    nodes.reserve(composed.size());
    for (const auto* manifest : composed) {
        DependencyNode node{.name = manifest->capability_name, .depends_on = {}};
        for (const auto& dependency : manifest->depends_on) {
            const bool is_known_capability = available.contains(dependency);
            if (!is_known_capability) {
                // A foundational runtime-library dependency (e.g. "entity",
                // "resource") - never itself a capability manifest, always
                // implicitly satisfied.
                continue;
            }
            if (!composed_names.contains(dependency)) {
                throw std::invalid_argument("host '" + host.host_name + "' composes '" +
                                            manifest->capability_name + "' but not its dependency '" +
                                            dependency + "'");
            }
            node.depends_on.push_back(dependency);
        }
        nodes.push_back(std::move(node));
    }

    const CompositionOrder order = resolve_composition_order(nodes);

    HostComposition composition{.host_name = host.host_name, .ordered_capabilities = {}};
    composition.ordered_capabilities.reserve(order.size());
    for (const auto& name : order) {
        composition.ordered_capabilities.push_back(*available.at(name));
    }

    return composition;
}

} // namespace atlas::cgen
