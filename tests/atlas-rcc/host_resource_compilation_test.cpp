#include "atlas/rcc/host_resource_compilation.hpp"
#include "atlas/rcc/resource_blob.hpp"
#include "atlas/rcc/resource_manifest.hpp"
#include "atlas/rcc/resource_table.hpp"
#include <atlas/resource/resource_id.hpp>

#include <filesystem>
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <string>

namespace atlas::rcc {
namespace {

// Set by CMakeLists.txt to the absolute path of tests/atlas-rcc/fixtures/ - real
// files on disk, matching CLAUDE.md's "test via a minimal test host, not by
// mocking behavior" rule.
constexpr std::string_view fixtures_dir = ATLAS_RCC_TEST_FIXTURES_DIR;

TEST(MergeResourceManifests, MergesDisjointNamesPreservingSourceThenAuthoredOrder) {
    const std::vector<ResourceManifestSource> sources{
        {"module_a.yaml",
         {
             {"a/mesh", "Mesh", "a/mesh.fbx", std::nullopt},
             {"a/texture", "Texture", "a/texture.png", std::nullopt},
         }},
        {"module_b.yaml",
         {
             {"b/mesh", "Mesh", "b/mesh.fbx", std::nullopt},
         }},
    };

    const std::vector<ResourceEntry> merged = merge_resource_manifests(sources);

    ASSERT_EQ(merged.size(), 3U);
    EXPECT_EQ(merged[0].name, "a/mesh");
    EXPECT_EQ(merged[1].name, "a/texture");
    EXPECT_EQ(merged[2].name, "b/mesh");
}

TEST(MergeResourceManifests, MergesAnEmptySourceListToAnEmptyEntryList) {
    EXPECT_TRUE(merge_resource_manifests({}).empty());
}

TEST(MergeResourceManifests, MergesASingleSourceUnchanged) {
    const std::vector<ResourceManifestSource> sources{
        {"module_a.yaml", {{"a/mesh", "Mesh", "a/mesh.fbx", std::nullopt}}},
    };

    const std::vector<ResourceEntry> merged = merge_resource_manifests(sources);

    ASSERT_EQ(merged.size(), 1U);
    EXPECT_EQ(merged[0].name, "a/mesh");
}

TEST(MergeResourceManifests, RejectsADuplicateNameAcrossTwoDifferentManifests) {
    const std::vector<ResourceManifestSource> sources{
        {"module_a.yaml", {{"shared/mesh", "Mesh", "a/mesh.fbx", std::nullopt}}},
        {"module_b.yaml", {{"shared/mesh", "Mesh", "b/mesh.fbx", std::nullopt}}},
    };

    try {
        (void)merge_resource_manifests(sources);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("shared/mesh"), std::string::npos);
        EXPECT_NE(message.find("module_a.yaml"), std::string::npos);
        EXPECT_NE(message.find("module_b.yaml"), std::string::npos);
    }
}

TEST(MergeResourceManifests, MergedThenPackedBlobIsByteIdenticalToOneCombinedManifest) {
    constexpr std::string_view manifest_a = R"(
resources:
  - name: hero/mesh
    type: Mesh
    path: hero.mesh
)";
    constexpr std::string_view manifest_b = R"(
resources:
  - name: hero/tex
    type: Mesh
    path: hero.tex
)";
    constexpr std::string_view combined_manifest = R"(
resources:
  - name: hero/mesh
    type: Mesh
    path: hero.mesh
  - name: hero/tex
    type: Mesh
    path: hero.tex
)";

    const std::vector<ResourceManifestSource> sources{
        {"a.yaml", parse_resource_manifest(manifest_a)},
        {"b.yaml", parse_resource_manifest(manifest_b)},
    };
    const std::vector<ResourceEntry> merged = merge_resource_manifests(sources);
    const std::vector<ResourceEntry> combined = parse_resource_manifest(combined_manifest);

    const std::filesystem::path fixtures{fixtures_dir};

    const auto to_compiled = [](const std::vector<ResourceEntry>& entries) {
        std::vector<CompiledResource> compiled;
        compiled.reserve(entries.size());
        for (const auto& entry : entries) {
            compiled.push_back(CompiledResource{
                atlas::ResourceId::from_name(entry.name), entry.name, entry.type, entry.path, std::nullopt});
        }
        return compiled;
    };

    const auto merged_blob = pack_resource_blob(to_compiled(merged), fixtures);
    const auto combined_blob = pack_resource_blob(to_compiled(combined), fixtures);

    EXPECT_EQ(merged_blob, combined_blob);
}

} // namespace
} // namespace atlas::rcc
