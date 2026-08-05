#include "atlas/rcc/resource_blob.hpp"
#include <atlas/resource/resource_id.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace atlas::rcc {
namespace {

// Set by CMakeLists.txt to the absolute path of tests/atlas-rcc/fixtures/ - real
// files on disk, matching CLAUDE.md's "test via a minimal test host, not by
// mocking behavior" rule.
constexpr std::string_view fixtures_dir = ATLAS_RCC_TEST_FIXTURES_DIR;

// Independently parses the documented blob format (u64 entry_count, then
// entry_count x {u64 id, u64 offset, u64 size}, then the concatenated data
// section) directly against the spec this test file documents - not by
// calling back into any reader this project might add later. Verifying the
// writer against the format itself (rather than round-tripping through a
// paired reader) is what actually proves pack_resource_blob is correct,
// matching resource_id_test.cpp's own "known reference values" precedent.
struct ParsedEntry {
    std::uint64_t id = 0;
    std::vector<std::byte> bytes;
};

std::uint64_t read_u64(const std::vector<std::byte>& blob, std::size_t offset) {
    std::uint64_t value = 0;
    std::memcpy(&value, blob.data() + offset, sizeof(value));
    return value;
}

std::vector<ParsedEntry> parse_blob(const std::vector<std::byte>& blob) {
    const std::uint64_t entry_count = read_u64(blob, 0);
    std::vector<ParsedEntry> entries;
    entries.reserve(entry_count);

    constexpr std::size_t index_entry_size = sizeof(std::uint64_t) * 3;
    const std::size_t data_start = sizeof(std::uint64_t) + (entry_count * index_entry_size);

    for (std::uint64_t i = 0; i < entry_count; ++i) {
        const std::size_t index_offset = sizeof(std::uint64_t) + (i * index_entry_size);
        const std::uint64_t id = read_u64(blob, index_offset);
        const std::uint64_t offset = read_u64(blob, index_offset + sizeof(std::uint64_t));
        const std::uint64_t size = read_u64(blob, index_offset + (2 * sizeof(std::uint64_t)));

        std::vector<std::byte> bytes(blob.begin() + static_cast<std::ptrdiff_t>(data_start + offset),
                                     blob.begin() + static_cast<std::ptrdiff_t>(data_start + offset + size));
        entries.push_back(ParsedEntry{id, std::move(bytes)});
    }

    return entries;
}

std::vector<std::byte> to_bytes(std::string_view text) {
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const char character : text) {
        bytes.push_back(static_cast<std::byte>(character));
    }
    return bytes;
}

TEST(PackResourceBlob, PacksEachEntrysFileBytesConcatenatedWithACorrectIndex) {
    const std::vector<CompiledResource> entries{
        {atlas::ResourceId::from_name("hero/mesh"), "hero/mesh", "Mesh", "hero.mesh", std::nullopt},
        {atlas::ResourceId::from_name("hero/tex"), "hero/tex", "Texture", "hero.tex", std::nullopt},
    };

    const std::vector<std::byte> blob = pack_resource_blob(entries, std::filesystem::path{fixtures_dir});
    const std::vector<ParsedEntry> parsed = parse_blob(blob);

    ASSERT_EQ(parsed.size(), 2U);
    EXPECT_EQ(parsed[0].id, atlas::ResourceId::from_name("hero/mesh").value);
    EXPECT_EQ(parsed[0].bytes, to_bytes("MESHBYTES"));
    EXPECT_EQ(parsed[1].id, atlas::ResourceId::from_name("hero/tex").value);
    EXPECT_EQ(parsed[1].bytes, to_bytes("TEXBYTES!!"));
}

TEST(PackResourceBlob, EmptyEntryListProducesJustTheHeader) {
    const std::vector<std::byte> blob = pack_resource_blob({}, std::filesystem::path{fixtures_dir});

    ASSERT_EQ(blob.size(), sizeof(std::uint64_t));
    EXPECT_EQ(read_u64(blob, 0), 0U);
}

TEST(PackResourceBlob, ThrowsWhenAnEntrysFileCannotBeRead) {
    const std::vector<CompiledResource> entries{
        {atlas::ResourceId::from_name("hero/missing"),
         "hero/missing",
         "Mesh",
         "does-not-exist.mesh",
         std::nullopt},
    };

    // (void) cast: pack_resource_blob is [[nodiscard]], and EXPECT_THROW's macro expansion otherwise
    // discards its result, which -Wunused-result (-Werror under Clang) treats as a real error.
    EXPECT_THROW((void)pack_resource_blob(entries, std::filesystem::path{fixtures_dir}),
                 std::invalid_argument);
}

} // namespace
} // namespace atlas::rcc
