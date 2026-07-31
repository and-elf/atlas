#include "atlas/resource/resource_registry.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace atlas::resource {
namespace {

// Set by CMakeLists.txt to the absolute path of tests/atlas-resource/fixtures/
// - real files on disk, not a mocked filesystem, per CLAUDE.md's "test via a
// minimal test host, not by mocking behavior" rule.
constexpr std::string_view fixtures_dir = ATLAS_RESOURCE_TEST_FIXTURES_DIR;

std::vector<std::byte> to_bytes(std::string_view text) {
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const char character : text) {
        bytes.push_back(static_cast<std::byte>(character));
    }
    return bytes;
}

ResourceRegistry make_registry() {
    std::vector<ResourceEntry> entries{
        {ResourceId::from_name("characters/hero/mesh"), "Mesh", "dummy.mesh"},
        {ResourceId::from_name("characters/hero/theme"), "Sound", "dummy.sound"},
        {ResourceId::from_name("characters/hero/missing"), "Mesh", "does-not-exist.mesh"},
        {ResourceId::from_name("characters/hero/broken"), "Mesh", "not-a-file"},
    };
    return ResourceRegistry{std::move(entries), std::filesystem::path{fixtures_dir}};
}

TEST(ResourceRegistry, UnresolvedWhenTypeWasNeverCompiled) {
    const ResourceRegistry registry = make_registry();

    const Resolution resolution = registry.resolve("Texture", ResourceId::from_name("characters/hero/mesh"));

    EXPECT_EQ(resolution.status, ResolutionStatus::Unresolved);
    EXPECT_TRUE(resolution.bytes.empty());
}

TEST(ResourceRegistry, UnresolvedWhenIdAbsentFromAKnownType) {
    const ResourceRegistry registry = make_registry();

    const Resolution resolution =
        registry.resolve("Mesh", ResourceId::from_name("characters/hero/never-declared"));

    EXPECT_EQ(resolution.status, ResolutionStatus::Unresolved);
}

TEST(ResourceRegistry, UnresolvedForTheNullId) {
    const ResourceRegistry registry = make_registry();

    const Resolution resolution = registry.resolve("Mesh", ResourceId{});

    EXPECT_EQ(resolution.status, ResolutionStatus::Unresolved);
}

TEST(ResourceRegistry, ResolvedReturnsExactFileBytes) {
    const ResourceRegistry registry = make_registry();

    const Resolution resolution = registry.resolve("Mesh", ResourceId::from_name("characters/hero/mesh"));

    ASSERT_EQ(resolution.status, ResolutionStatus::Resolved);
    EXPECT_EQ(resolution.bytes, to_bytes("ATLASMESHFIXTURE\n"));
}

TEST(ResourceRegistry, ResolutionFailedWhenTheFileCannotBeRead) {
    const ResourceRegistry registry = make_registry();

    const Resolution resolution = registry.resolve("Mesh", ResourceId::from_name("characters/hero/missing"));

    EXPECT_EQ(resolution.status, ResolutionStatus::ResolutionFailed);
    EXPECT_TRUE(resolution.bytes.empty());
}

TEST(ResourceRegistry, ResolutionFailedWhenThePathIsNotARegularFile) {
    // "not-a-file" is a real directory under fixtures/ - std::filesystem::file_size succeeds on a
    // directory (it has a stat-able size), but opening it as an ifstream fails, exercising the
    // distinct failure branch that occurs after a successful stat.
    const ResourceRegistry registry = make_registry();

    const Resolution resolution = registry.resolve("Mesh", ResourceId::from_name("characters/hero/broken"));

    EXPECT_EQ(resolution.status, ResolutionStatus::ResolutionFailed);
}

TEST(ResourceRegistry, EntriesPartitionByTypeWithNoCrossContamination) {
    const ResourceRegistry registry = make_registry();

    // The hero's theme was compiled as "Sound", never as "Mesh" - asking the
    // Mesh partition for it must not find it just because the id exists
    // somewhere in the registry.
    const Resolution as_mesh = registry.resolve("Mesh", ResourceId::from_name("characters/hero/theme"));
    const Resolution as_sound = registry.resolve("Sound", ResourceId::from_name("characters/hero/theme"));

    EXPECT_EQ(as_mesh.status, ResolutionStatus::Unresolved);
    EXPECT_EQ(as_sound.status, ResolutionStatus::Resolved);
}

TEST(ResourceRegistry, RepeatedResolveCallsProduceBitIdenticalOutput) {
    const ResourceRegistry registry = make_registry();
    const auto id = ResourceId::from_name("characters/hero/mesh");

    const Resolution first = registry.resolve("Mesh", id);
    const Resolution second = registry.resolve("Mesh", id);

    EXPECT_EQ(first.status, second.status);
    EXPECT_EQ(first.bytes, second.bytes);
}

} // namespace
} // namespace atlas::resource
