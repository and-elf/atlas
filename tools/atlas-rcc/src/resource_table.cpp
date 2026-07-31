#include "atlas/rcc/resource_table.hpp"

namespace atlas::rcc {

ResourceTable compile_resource_table(const std::vector<ResourceEntry>& entries) {
    ResourceTable table;
    table.reserve(entries.size());

    for (const auto& entry : entries) {
        const auto id = atlas::ResourceId::from_name(entry.name);
        table.emplace(id, CompiledResource{id, entry.name, entry.type, entry.path});
    }

    return table;
}

} // namespace atlas::rcc
