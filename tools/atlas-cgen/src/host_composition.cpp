#include "atlas/cgen/host_composition.hpp"

#include "atlas/cgen/dependency_graph.hpp"
#include "atlas/cgen/property_graph.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace atlas::cgen {

namespace {

// Builds one manifest's DependencyNode: its explicit depends_on edges
// (validated against the composed closure) plus any derived property edges
// (property_graph.hpp) - split out of resolve_host_composition itself purely
// to keep that function's cognitive complexity down; the merge logic here
// isn't reused anywhere else.
DependencyNode
build_dependency_node(const HostManifest& host,
                      const Manifest& manifest,
                      const std::unordered_map<std::string, const Manifest*>& available,
                      const std::unordered_set<std::string>& composed_names,
                      const std::unordered_map<std::string, std::vector<std::string>>& property_edges) {
    DependencyNode node{.name = manifest.capability_name, .depends_on = {}};
    for (const auto& dependency : manifest.depends_on) {
        const bool is_known_capability = available.contains(dependency);
        if (!is_known_capability) {
            // A foundational runtime-library dependency (e.g. "entity",
            // "resource") - never itself a capability manifest, always
            // implicitly satisfied.
            continue;
        }
        if (!composed_names.contains(dependency)) {
            throw std::invalid_argument("host '" + host.host_name + "' composes '" +
                                        manifest.capability_name + "' but not its dependency '" + dependency +
                                        "'");
        }
        node.depends_on.push_back(dependency);
    }

    const auto derived = property_edges.find(manifest.capability_name);
    if (derived != property_edges.end()) {
        for (const auto& provider : derived->second) {
            if (std::find(node.depends_on.begin(), node.depends_on.end(), provider) ==
                node.depends_on.end()) {
                node.depends_on.push_back(provider);
            }
        }
    }

    return node;
}

} // namespace

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

    // Property-derived edges (issue #16: "systems coupled to data, not
    // implementation") - resolved against the composed set only, mirroring
    // how depends_on closure validation above is already scoped to it. This
    // needs a value copy (composed holds pointers into available_manifests,
    // not necessarily contiguous) rather than an extra span overload, kept
    // simple since manifests aren't large.
    std::vector<Manifest> composed_values;
    composed_values.reserve(composed.size());
    for (const auto* manifest : composed) {
        composed_values.push_back(*manifest);
    }
    const auto property_providers = resolve_property_providers(composed_values);
    const auto property_edges = resolve_property_dependency_edges(composed_values, property_providers);

    std::vector<DependencyNode> nodes;
    nodes.reserve(composed.size());
    for (const auto* manifest : composed) {
        nodes.push_back(build_dependency_node(host, *manifest, available, composed_names, property_edges));
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
