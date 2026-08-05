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

TEST(ParseResourceManifest, EntryWithoutAnimationBlockHasNoAnimationMetadata) {
    const std::vector<ResourceEntry> entries = parse_resource_manifest(hero_manifest);

    ASSERT_EQ(entries.size(), 2U);
    EXPECT_FALSE(entries[0].animation.has_value());
    EXPECT_FALSE(entries[1].animation.has_value());
}

TEST(ParseResourceManifest, ParsesAnimationBlockWhenPresent) {
    // The design decision this test locks in: `animation:` is parsed
    // whenever present, regardless of what string `type` holds - this tool
    // deliberately has no closed vocabulary to gate that parsing on (see
    // README's "Real discoveries" section on `type` being open-ended).
    constexpr std::string_view run_cycle = R"(
resources:
  - name: characters/hero/run_cycle
    type: Animation
    path: characters/hero/run_cycle.fbx
    animation:
      skeleton: characters/hero/skeleton
      loop: true
      playback_rate: 1.5
)";

    const std::vector<ResourceEntry> entries = parse_resource_manifest(run_cycle);

    ASSERT_EQ(entries.size(), 1U);
    ASSERT_TRUE(entries[0].animation.has_value());
    EXPECT_EQ(entries[0].animation->skeleton, "characters/hero/skeleton");
    EXPECT_TRUE(entries[0].animation->loop);
    EXPECT_DOUBLE_EQ(entries[0].animation->playback_rate, 1.5);
}

TEST(ParseResourceManifest, ParsesAnimationBlockOnAnyTypeNotOnlyAnimation) {
    constexpr std::string_view non_animation_type = R"(
resources:
  - name: characters/hero/idle_pose
    type: Pose
    path: characters/hero/idle_pose.fbx
    animation:
      skeleton: characters/hero/skeleton
)";

    const std::vector<ResourceEntry> entries = parse_resource_manifest(non_animation_type);

    ASSERT_EQ(entries.size(), 1U);
    ASSERT_TRUE(entries[0].animation.has_value());
    EXPECT_EQ(entries[0].animation->skeleton, "characters/hero/skeleton");
}

TEST(ParseResourceManifest, AnimationBlockDefaultsLoopAndPlaybackRateWhenOmitted) {
    constexpr std::string_view skeleton_only = R"(
resources:
  - name: characters/hero/run_cycle
    type: Animation
    path: characters/hero/run_cycle.fbx
    animation:
      skeleton: characters/hero/skeleton
)";

    const std::vector<ResourceEntry> entries = parse_resource_manifest(skeleton_only);

    ASSERT_EQ(entries.size(), 1U);
    ASSERT_TRUE(entries[0].animation.has_value());
    EXPECT_FALSE(entries[0].animation->loop);
    EXPECT_DOUBLE_EQ(entries[0].animation->playback_rate, 1.0);
}

TEST(ParseResourceManifest, RejectsAnimationBlockThatIsNotAMapping) {
    constexpr std::string_view animation_is_scalar = R"(
resources:
  - name: characters/hero/run_cycle
    type: Animation
    path: characters/hero/run_cycle.fbx
    animation: characters/hero/skeleton
)";

    EXPECT_THROW({ (void)parse_resource_manifest(animation_is_scalar); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsAnimationBlockMissingSkeleton) {
    constexpr std::string_view missing_skeleton = R"(
resources:
  - name: characters/hero/run_cycle
    type: Animation
    path: characters/hero/run_cycle.fbx
    animation:
      loop: true
)";

    EXPECT_THROW({ (void)parse_resource_manifest(missing_skeleton); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsAnimationBlockWithEmptySkeleton) {
    constexpr std::string_view empty_skeleton = R"(
resources:
  - name: characters/hero/run_cycle
    type: Animation
    path: characters/hero/run_cycle.fbx
    animation:
      skeleton: ""
)";

    EXPECT_THROW({ (void)parse_resource_manifest(empty_skeleton); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsAnimationLoopThatIsNotABoolean) {
    constexpr std::string_view loop_not_bool = R"(
resources:
  - name: characters/hero/run_cycle
    type: Animation
    path: characters/hero/run_cycle.fbx
    animation:
      skeleton: characters/hero/skeleton
      loop: sideways
)";

    EXPECT_THROW({ (void)parse_resource_manifest(loop_not_bool); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsAnimationPlaybackRateThatIsNotANumber) {
    constexpr std::string_view rate_not_number = R"(
resources:
  - name: characters/hero/run_cycle
    type: Animation
    path: characters/hero/run_cycle.fbx
    animation:
      skeleton: characters/hero/skeleton
      playback_rate: fast
)";

    EXPECT_THROW({ (void)parse_resource_manifest(rate_not_number); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsZeroPlaybackRate) {
    constexpr std::string_view rate_zero = R"(
resources:
  - name: characters/hero/run_cycle
    type: Animation
    path: characters/hero/run_cycle.fbx
    animation:
      skeleton: characters/hero/skeleton
      playback_rate: 0
)";

    EXPECT_THROW({ (void)parse_resource_manifest(rate_zero); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsNegativePlaybackRate) {
    constexpr std::string_view rate_negative = R"(
resources:
  - name: characters/hero/run_cycle
    type: Animation
    path: characters/hero/run_cycle.fbx
    animation:
      skeleton: characters/hero/skeleton
      playback_rate: -1.0
)";

    EXPECT_THROW({ (void)parse_resource_manifest(rate_negative); }, std::invalid_argument);
}

TEST(ParseResourceManifest, RejectsAnimationBlockWithUnrecognizedField) {
    constexpr std::string_view unknown_animation_field = R"(
resources:
  - name: characters/hero/run_cycle
    type: Animation
    path: characters/hero/run_cycle.fbx
    animation:
      skeleton: characters/hero/skeleton
      retarget: true
)";

    EXPECT_THROW({ (void)parse_resource_manifest(unknown_animation_field); }, std::invalid_argument);
}

} // namespace
} // namespace atlas::rcc
