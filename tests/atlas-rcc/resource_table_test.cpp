#include "atlas/rcc/resource_table.hpp"
#include <atlas/resource/resource_id.hpp>

#include <gtest/gtest.h>
#include <optional>

namespace atlas::rcc {
namespace {

TEST(CompileResourceTable, CompilesEachEntryKeyedByItsResourceId) {
    const std::vector<ResourceEntry> entries{
        {"characters/hero/mesh", "Mesh", "characters/hero/mesh.fbx", std::nullopt},
        {"characters/hero/texture", "Texture", "characters/hero/diffuse.png", std::nullopt},
    };

    const ResourceTable table = compile_resource_table(entries);

    ASSERT_EQ(table.size(), 2U);

    const auto mesh_id = atlas::ResourceId::from_name("characters/hero/mesh");
    const auto texture_id = atlas::ResourceId::from_name("characters/hero/texture");

    ASSERT_TRUE(table.contains(mesh_id));
    EXPECT_EQ(table.at(mesh_id).name, "characters/hero/mesh");
    EXPECT_EQ(table.at(mesh_id).type, "Mesh");
    EXPECT_EQ(table.at(mesh_id).path, "characters/hero/mesh.fbx");

    ASSERT_TRUE(table.contains(texture_id));
    EXPECT_EQ(table.at(texture_id).name, "characters/hero/texture");
    EXPECT_EQ(table.at(texture_id).type, "Texture");
    EXPECT_EQ(table.at(texture_id).path, "characters/hero/diffuse.png");
}

TEST(CompileResourceTable, CompilesAnEmptyEntryListToAnEmptyTable) {
    const ResourceTable table = compile_resource_table({});

    EXPECT_TRUE(table.empty());
}

TEST(CompileResourceTable, EntryWithoutAnimationMetadataCompilesWithNoAnimationField) {
    const std::vector<ResourceEntry> entries{
        {"characters/hero/mesh", "Mesh", "characters/hero/mesh.fbx", std::nullopt},
    };

    const ResourceTable table = compile_resource_table(entries);

    const auto mesh_id = atlas::ResourceId::from_name("characters/hero/mesh");
    ASSERT_TRUE(table.contains(mesh_id));
    EXPECT_FALSE(table.at(mesh_id).animation.has_value());
}

TEST(CompileResourceTable, ResolvesAnimationSkeletonNameToAResourceId) {
    const std::vector<ResourceEntry> entries{
        {"characters/hero/run_cycle",
         "Animation",
         "characters/hero/run_cycle.fbx",
         AnimationMetadata{"characters/hero/skeleton", true, 1.5}},
    };

    const ResourceTable table = compile_resource_table(entries);

    const auto run_cycle_id = atlas::ResourceId::from_name("characters/hero/run_cycle");
    ASSERT_TRUE(table.contains(run_cycle_id));

    const auto& compiled = table.at(run_cycle_id);
    ASSERT_TRUE(compiled.animation.has_value());
    EXPECT_EQ(compiled.animation->skeleton, atlas::ResourceId::from_name("characters/hero/skeleton"));
    EXPECT_TRUE(compiled.animation->loop);
    EXPECT_DOUBLE_EQ(compiled.animation->playback_rate, 1.5);
}

TEST(CompileResourceTable, DistinctNamesNeverCollideInPractice) {
    // Not a proof of no collisions - a sanity check that two clearly
    // distinct authored names land in two distinct table slots under
    // real use, exercising the same lookup path a real host would use.
    const std::vector<ResourceEntry> entries{
        {"a", "Mesh", "a.fbx", std::nullopt},
        {"b", "Mesh", "b.fbx", std::nullopt},
    };

    const ResourceTable table = compile_resource_table(entries);

    EXPECT_NE(atlas::ResourceId::from_name("a"), atlas::ResourceId::from_name("b"));
    EXPECT_EQ(table.size(), 2U);
}

} // namespace
} // namespace atlas::rcc
