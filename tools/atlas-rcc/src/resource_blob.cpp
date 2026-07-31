#include "atlas/rcc/resource_blob.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>
#include <system_error>

namespace atlas::rcc {

namespace {

std::vector<std::byte> read_whole_file(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        throw std::invalid_argument("atlas-rcc: cannot read resource file '" + path.string() +
                                    "': " + error.message());
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::invalid_argument("atlas-rcc: cannot open resource file '" + path.string() + "'");
    }

    std::vector<std::byte> bytes(size);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - istream::read needs a char*.
    auto* const data = reinterpret_cast<char*>(bytes.data());
    if (!file.read(data, static_cast<std::streamsize>(bytes.size()))) {
        throw std::invalid_argument("atlas-rcc: failed reading resource file '" + path.string() + "'");
    }

    return bytes;
}

void append_u64(std::vector<std::byte>& blob, std::uint64_t value) {
    std::array<std::byte, sizeof(value)> raw{};
    std::memcpy(raw.data(), &value, sizeof(value));
    blob.insert(blob.end(), raw.begin(), raw.end());
}

} // namespace

std::vector<std::byte> pack_resource_blob(const std::vector<CompiledResource>& entries,
                                          const std::filesystem::path& asset_root) {
    std::vector<std::vector<std::byte>> file_bytes;
    file_bytes.reserve(entries.size());
    for (const auto& entry : entries) {
        file_bytes.push_back(read_whole_file(asset_root / entry.path));
    }

    std::vector<std::byte> blob;
    append_u64(blob, entries.size());

    std::uint64_t running_offset = 0;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        append_u64(blob, entries[i].id.value);
        append_u64(blob, running_offset);
        append_u64(blob, file_bytes[i].size());
        running_offset += file_bytes[i].size();
    }

    for (const auto& bytes : file_bytes) {
        blob.insert(blob.end(), bytes.begin(), bytes.end());
    }

    return blob;
}

} // namespace atlas::rcc
