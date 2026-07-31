#include "atlas/resource/resource_registry.hpp"

#include <cstring>
#include <fstream>
#include <ios>
#include <system_error>
#include <utility>

namespace atlas::resource {

namespace {

constexpr std::size_t index_entry_size = sizeof(std::uint64_t) * 3;

std::uint64_t read_u64(const std::vector<std::byte>& bytes, std::size_t offset) {
    std::uint64_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

std::optional<std::vector<std::byte>> read_whole_file(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        return std::nullopt;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::vector<std::byte> bytes(size);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - istream::read needs a char*.
    auto* const data = reinterpret_cast<char*>(bytes.data());
    if (!file.read(data, static_cast<std::streamsize>(bytes.size()))) {
        return std::nullopt;
    }

    return bytes;
}

} // namespace

std::optional<ResourceRegistry::LoadedBlob> ResourceRegistry::load_blob(const std::filesystem::path& path) {
    std::optional<std::vector<std::byte>> maybe_bytes = read_whole_file(path);
    if (!maybe_bytes) {
        return std::nullopt;
    }
    std::vector<std::byte> bytes = std::move(*maybe_bytes);

    if (bytes.size() < sizeof(std::uint64_t)) {
        return std::nullopt;
    }
    const std::uint64_t entry_count = read_u64(bytes, 0);

    const std::size_t index_bytes = static_cast<std::size_t>(entry_count) * index_entry_size;
    if (bytes.size() < sizeof(std::uint64_t) + index_bytes) {
        return std::nullopt;
    }
    const std::size_t data_start = sizeof(std::uint64_t) + index_bytes;
    const std::size_t available = bytes.size() - data_start;

    std::unordered_map<ResourceId, BlobEntry> index;
    index.reserve(entry_count);
    for (std::uint64_t i = 0; i < entry_count; ++i) {
        const std::size_t index_offset =
            sizeof(std::uint64_t) + (static_cast<std::size_t>(i) * index_entry_size);
        const std::uint64_t id = read_u64(bytes, index_offset);
        const std::uint64_t offset = read_u64(bytes, index_offset + sizeof(std::uint64_t));
        const std::uint64_t size = read_u64(bytes, index_offset + (2 * sizeof(std::uint64_t)));

        if (offset > available || size > available - offset) {
            return std::nullopt; // truncated/malformed - an entry claims data past the buffer's end
        }

        index.emplace(ResourceId{id}, BlobEntry{data_start + offset, size});
    }

    return LoadedBlob{std::move(bytes), std::move(index)};
}

ResourceRegistry::ResourceRegistry(
    const std::unordered_map<std::string, std::filesystem::path>& blob_paths_by_type) {
    for (const auto& [type_name, path] : blob_paths_by_type) {
        blobs_by_type_.emplace(type_name, load_blob(path));
    }
}

Resolution ResourceRegistry::resolve(std::string_view type_name, ResourceId id) const {
    if (id.is_null()) {
        return Resolution{};
    }

    const auto type_it = blobs_by_type_.find(std::string(type_name));
    if (type_it == blobs_by_type_.end()) {
        return Resolution{};
    }

    const std::optional<LoadedBlob>& maybe_blob = type_it->second;
    if (!maybe_blob.has_value()) {
        return Resolution{ResolutionStatus::ResolutionFailed, {}};
    }

    const LoadedBlob& blob = maybe_blob.value();
    const auto entry_it = blob.index.find(id);
    if (entry_it == blob.index.end()) {
        return Resolution{};
    }

    const BlobEntry& entry = entry_it->second;
    std::vector<std::byte> bytes(blob.bytes.begin() + static_cast<std::ptrdiff_t>(entry.offset),
                                 blob.bytes.begin() + static_cast<std::ptrdiff_t>(entry.offset + entry.size));

    return Resolution{ResolutionStatus::Resolved, std::move(bytes)};
}

} // namespace atlas::resource
