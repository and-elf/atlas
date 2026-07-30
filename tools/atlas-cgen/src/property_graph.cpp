#include "atlas/cgen/property_graph.hpp"

namespace atlas::cgen {

std::unordered_map<std::string, std::string> resolve_property_providers(std::span<const Manifest> manifests) {
    std::unordered_map<std::string, std::string> providers;
    for (const auto& manifest : manifests) {
        for (const auto& property : manifest.properties) {
            const auto [it, inserted] = providers.try_emplace(property.name, manifest.capability_name);
            if (!inserted) {
                throw PropertyProviderConflictError("property '" + property.name +
                                                    "' has multiple providers: '" + it->second + "', '" +
                                                    manifest.capability_name + "'");
            }
        }
    }
    return providers;
}

std::unordered_map<std::string, std::vector<std::string>>
resolve_property_dependency_edges(std::span<const Manifest> composed,
                                  const std::unordered_map<std::string, std::string>& providers) {
    std::unordered_map<std::string, std::vector<std::string>> edges;
    for (const auto& manifest : composed) {
        std::vector<std::string> dependencies;
        for (const auto& property : manifest.consumes) {
            const auto it = providers.find(property);
            if (it == providers.end()) {
                throw UnresolvedPropertyConsumerError("capability '" + manifest.capability_name +
                                                      "' consumes property '" + property +
                                                      "' but no composed capability provides it");
            }
            if (it->second != manifest.capability_name) {
                dependencies.push_back(it->second);
            }
        }
        if (!dependencies.empty()) {
            edges.emplace(manifest.capability_name, std::move(dependencies));
        }
    }
    return edges;
}

} // namespace atlas::cgen
