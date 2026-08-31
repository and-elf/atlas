#include "atlas/rcc/host_resource_compilation.hpp"

#include <stdexcept>
#include <unordered_map>

namespace atlas::rcc {

std::vector<ResourceEntry> merge_resource_manifests(const std::vector<ResourceManifestSource>& sources) {
    std::vector<ResourceEntry> merged;
    std::unordered_map<std::string, std::string> owning_label_by_name;

    for (const auto& source : sources) {
        for (const auto& entry : source.entries) {
            const auto [it, inserted] = owning_label_by_name.emplace(entry.name, source.label);
            if (!inserted) {
                throw std::invalid_argument("resource name '" + entry.name + "' is declared in both '" +
                                            it->second + "' and '" + source.label + "'");
            }
            merged.push_back(entry);
        }
    }

    return merged;
}

} // namespace atlas::rcc
