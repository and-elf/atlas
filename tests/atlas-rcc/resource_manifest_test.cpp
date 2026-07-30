#include "atlas/rcc/resource_manifest.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace atlas::rcc {
namespace {

constexpr std::string_view hero_manifest = R"(
resources:
  - name: characters/hero/mesh
    type: Mesh
    path: characters/hero/mesh.fbx
  - name: characters/hero/texture
    type: Texture
    path: characters/hero/diffuse.png
)";

TEST(ParseResourceManifest, ParsesAnOrderedListOfResourceEntries) {
    const std::vector<ResourceEntry> entries = parse_resource_manifest(hero_manifest);

    ASSERT_EQ(entries.size(), 2U);
    EXPECT_EQ(entries[0].name, "characters/hero/mesh");
    EXPECT_EQ(entries[0].type, "Mesh");
    EXPECT_EQ(entries[0].path, "characters/hero/mesh.fbx");
    EXPECT_EQ(entries[1].name, "characters/hero/texture");
    EXPECT_EQ(entries[1].type, "Texture");
    EXPECT_EQ(entries[1].path, "characters/hero/diffuse.png");
}

TEST(ParseResourceManifest, RejectsMalformedYaml) {
    constexpr std::string_view malformed = "resources: [this is not: - valid";

    EXPECT_THROW({ (void)parse_resource_manifest(malformed); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsNonMappingDocumentRoot) {
    constexpr std::string_view sequence_root = "- just\n- a\n- sequence\n";

    EXPECT_THROW({ (void)parse_resource_manifest(sequence_root); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsMissingResourcesBlock) {
    constexpr std::string_view no_resources = "unrelated: value\n";

    EXPECT_THROW({ (void)parse_resource_manifest(no_resources); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsResourcesBlockThatIsNotASequence) {
    constexpr std::string_view resources_is_map = R"(
resources:
  name: characters/hero/mesh
)";

    EXPECT_THROW({ (void)parse_resource_manifest(resources_is_map); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsEntryThatIsNotAMapping) {
    constexpr std::string_view entry_is_scalar = R"(
resources:
  - characters/hero/mesh
)";

    EXPECT_THROW({ (void)parse_resource_manifest(entry_is_scalar); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsEntryMissingName) {
    constexpr std::string_view missing_name = R"(
resources:
  - type: Mesh
    path: characters/hero/mesh.fbx
)";

    EXPECT_THROW({ (void)parse_resource_manifest(missing_name); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsEntryMissingType) {
    constexpr std::string_view missing_type = R"(
resources:
  - name: characters/hero/mesh
    path: characters/hero/mesh.fbx
)";

    EXPECT_THROW({ (void)parse_resource_manifest(missing_type); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsEntryMissingPath) {
    constexpr std::string_view missing_path = R"(
resources:
  - name: characters/hero/mesh
    type: Mesh
)";

    EXPECT_THROW({ (void)parse_resource_manifest(missing_path); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsEntryWithUnrecognizedField) {
    constexpr std::string_view unknown_field = R"(
resources:
  - name: characters/hero/mesh
    type: Mesh
    path: characters/hero/mesh.fbx
    tags: [cosmetic]
)";

    EXPECT_THROW({ (void)parse_resource_manifest(unknown_field); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsEntryWithEmptyName) {
    constexpr std::string_view empty_name = R"(
resources:
  - name: ""
    type: Mesh
    path: characters/hero/mesh.fbx
)";

    EXPECT_THROW({ (void)parse_resource_manifest(empty_name); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsDuplicateResourceNames) {
    constexpr std::string_view duplicate_name = R"(
resources:
  - name: characters/hero/mesh
    type: Mesh
    path: characters/hero/mesh.fbx
  - name: characters/hero/mesh
    type: Mesh
    path: characters/hero/mesh_lod1.fbx
)";

    EXPECT_THROW({ (void)parse_resource_manifest(duplicate_name); }, std::invalid_argument);
}

TEST(ParseResourceManifest, ParsesAnEmptyResourcesListAsNoEntries) {
    constexpr std::string_view empty_list = "resources: []\n";

    const std::vector<ResourceEntry> entries = parse_resource_manifest(empty_list);

    EXPECT_TRUE(entries.empty());
}

} // namespace
} // namespace atlas::rcc
