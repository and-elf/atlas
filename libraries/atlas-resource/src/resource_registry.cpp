#include "atlas/resource/resource_registry.hpp"

#include <fstream>
#include <ios>
#include <system_error>
#include <utility>

namespace atlas::resource {

ResourceRegistry::ResourceRegistry(std::vector<ResourceEntry> entries, std::filesystem::path root)
    : root_(std::move(root)) {
    for (auto& entry : entries) {
        paths_by_type_[entry.type].emplace(entry.id, std::move(entry.path));
    }
}

Resolution ResourceRegistry::resolve(std::string_view type_name, ResourceId id) const {
    if (id.is_null()) {
        return Resolution{};
    }

    const auto type_it = paths_by_type_.find(std::string(type_name));
    if (type_it == paths_by_type_.end()) {
        return Resolution{};
    }

    const auto& ids_of_type = type_it->second;
    const auto id_it = ids_of_type.find(id);
    if (id_it == ids_of_type.end()) {
        return Resolution{};
    }

    const std::filesystem::path file_path = root_ / id_it->second;

    // Non-throwing overload: a missing asset at runtime is an ordinary condition this function
    // reports through its return value (ResolutionFailed), never an exception - see the header's
    // own reasoning for why this differs from the parse-time exceptions atlas-cgen/atlas-rcc throw.
    std::error_code error;
    const auto size = std::filesystem::file_size(file_path, error);
    if (error) {
        return Resolution{ResolutionStatus::ResolutionFailed, {}};
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        return Resolution{ResolutionStatus::ResolutionFailed, {}};
    }

    std::vector<std::byte> bytes(size);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - istream::read needs a char*.
    auto* const data = reinterpret_cast<char*>(bytes.data());
    if (!file.read(data, static_cast<std::streamsize>(bytes.size()))) {
        return Resolution{ResolutionStatus::ResolutionFailed, {}};
    }

    return Resolution{ResolutionStatus::Resolved, std::move(bytes)};
}

} // namespace atlas::resource
