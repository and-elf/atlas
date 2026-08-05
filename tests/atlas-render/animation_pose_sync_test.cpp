#include "atlas/render/animation_pose_sync.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace atlas::render {
namespace {

constexpr std::string_view fixtures_dir = ATLAS_RENDER_TEST_FIXTURES_DIR;

std::vector<std::byte> read_fixture(const std::string& name) {
    const std::filesystem::path path = std::filesystem::path{fixtures_dir} / name;
    std::ifstream file(path, std::ios::binary);
    std::vector<char> raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    std::vector<std::byte> bytes(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        bytes[i] = static_cast<std::byte>(raw[i]);
    }
    return bytes;
}

void append_bytes(std::vector<std::byte>& out, const void* data, std::size_t size) {
    const std::size_t offset = out.size();
    out.resize(offset + size);
    std::memcpy(out.data() + offset, data, size);
}

void append_u32(std::vector<std::byte>& out, std::uint32_t value) {
    append_bytes(out, &value, sizeof(value));
}
void append_u64(std::vector<std::byte>& out, std::uint64_t value) {
    append_bytes(out, &value, sizeof(value));
}
void append_f32(std::vector<std::byte>& out, float value) {
    append_bytes(out, &value, sizeof(value));
}

// Hand-packs a well-formed two_joint_two_keyframe.animation-compatible
// skeleton (2 joints, joint 1 a child of joint 0) against skeleton_asset.hpp's
// own format - mirroring skeleton_asset_test.cpp's own fixture-construction
// approach, just inline rather than a checked-in file since this test file
// only ever needs this one shape.
std::vector<std::byte> pack_two_joint_skeleton_bytes() {
    std::vector<std::byte> bytes;
    append_u32(bytes, 2); // joint_count

    // joint 0: root
    append_u32(bytes, std::numeric_limits<std::uint32_t>::max());
    append_f32(bytes, 0.0F);
    append_f32(bytes, 0.0F);
    append_f32(bytes, 0.0F); // position
    append_f32(bytes, 0.0F);
    append_f32(bytes, 0.0F);
    append_f32(bytes, 0.0F);
    append_f32(bytes, 1.0F); // rotation
    append_f32(bytes, 1.0F);
    append_f32(bytes, 1.0F);
    append_f32(bytes, 1.0F); // scale

    // joint 1: child of joint 0
    append_u32(bytes, 0);
    append_f32(bytes, 0.0F);
    append_f32(bytes, 0.0F);
    append_f32(bytes, 0.0F);
    append_f32(bytes, 0.0F);
    append_f32(bytes, 0.0F);
    append_f32(bytes, 0.0F);
    append_f32(bytes, 1.0F);
    append_f32(bytes, 1.0F);
    append_f32(bytes, 1.0F);
    append_f32(bytes, 1.0F);

    return bytes;
}

std::vector<std::byte> pack_single_entry_blob(ResourceId id, const std::vector<std::byte>& data) {
    std::vector<std::byte> blob;
    append_u64(blob, 1);
    append_u64(blob, id.value);
    append_u64(blob, 0);
    append_u64(blob, data.size());
    blob.insert(blob.end(), data.begin(), data.end());
    return blob;
}

std::filesystem::path write_temp_blob(const std::string& name, const std::vector<std::byte>& bytes) {
    const std::filesystem::path path = std::filesystem::path{::testing::TempDir()} / name;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - ostream::write needs a const char*.
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return path;
}

// A registry with one clip (two_joint_two_keyframe.animation, from
// animation_asset_test.cpp's own fixture) under "Animation" and one matching
// two-joint skeleton under "Skeleton", both keyed by the ids passed in - a
// free function rather than a test fixture class, mirroring
// animation_decode_cache_test.cpp's own plain-TEST()-with-locals style
// (avoids a fixture's protected member variables entirely, which
// cppcoreguidelines-non-private-member-variables-in-classes/
// readability-identifier-naming both flag on this project's clang-tidy
// configuration).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) - both ResourceId, but every
// call site below names its arguments via the local clip_id/skeleton_id it just
// constructed immediately above the call, so a transposition would be caught by every
// test's own subsequent assertions rather than silently passing.
resource::ResourceRegistry make_registry(ResourceId clip_id, ResourceId skeleton_id) {
    const auto clip_blob_path =
        write_temp_blob("pose_sync_clip.blob",
                        pack_single_entry_blob(clip_id, read_fixture("two_joint_two_keyframe.animation")));
    const auto skeleton_blob_path = write_temp_blob(
        "pose_sync_skeleton.blob", pack_single_entry_blob(skeleton_id, pack_two_joint_skeleton_bytes()));
    return resource::ResourceRegistry{{{"Animation", clip_blob_path}, {"Skeleton", skeleton_blob_path}}};
}

TEST(SyncAnimationPoses, EntityWithNoCurrentAnimationIsSkippedAndNoPoseIsWritten) {
    const ResourceId clip_id = ResourceId::from_name("animations/pose-sync/clip-1");
    const ResourceId skeleton_id = ResourceId::from_name("skeletons/pose-sync/skeleton-1");
    const EntityRef entity{1};
    const resource::ResourceRegistry registry = make_registry(clip_id, skeleton_id);
    AnimationDecodeCache animation_cache{registry, "Animation"};
    const runtime::PropertyStore<CurrentAnimation> current_animations;
    const runtime::PropertyStore<AnimationPlaybackConfig> playback_configs;
    runtime::PropertyStore<AnimationClipTime> clip_times;
    runtime::PropertyStore<AnimationPose> poses;
    const std::vector<EntityRef> entities{entity};

    sync_animation_poses(entities,
                         current_animations,
                         playback_configs,
                         clip_times,
                         poses,
                         animation_cache,
                         registry,
                         "Skeleton",
                         0.5);

    EXPECT_FALSE(poses.get(entity).has_value());
    EXPECT_FALSE(clip_times.get(entity).has_value());
}

TEST(SyncAnimationPoses, EntityWithNoPlaybackConfigStillAdvancesElapsedTimeButWritesNoPose) {
    const ResourceId clip_id = ResourceId::from_name("animations/pose-sync/clip-2");
    const ResourceId skeleton_id = ResourceId::from_name("skeletons/pose-sync/skeleton-2");
    const EntityRef entity{1};
    const resource::ResourceRegistry registry = make_registry(clip_id, skeleton_id);
    AnimationDecodeCache animation_cache{registry, "Animation"};
    runtime::PropertyStore<CurrentAnimation> current_animations;
    current_animations.set(entity, CurrentAnimation{.clip = clip_id});
    const runtime::PropertyStore<AnimationPlaybackConfig> playback_configs;
    runtime::PropertyStore<AnimationClipTime> clip_times;
    runtime::PropertyStore<AnimationPose> poses;
    const std::vector<EntityRef> entities{entity};

    sync_animation_poses(entities,
                         current_animations,
                         playback_configs,
                         clip_times,
                         poses,
                         animation_cache,
                         registry,
                         "Skeleton",
                         0.5);

    EXPECT_FALSE(poses.get(entity).has_value());
    ASSERT_TRUE(clip_times.get(entity).has_value());
    EXPECT_DOUBLE_EQ(clip_times.get(entity)->get().elapsed_seconds, 0.5);
}

TEST(SyncAnimationPoses, FullyConfiguredEntityGetsAPoseWrittenMatchingSampleAnimationPose) {
    const ResourceId clip_id = ResourceId::from_name("animations/pose-sync/clip-3");
    const ResourceId skeleton_id = ResourceId::from_name("skeletons/pose-sync/skeleton-3");
    const EntityRef entity{1};
    const resource::ResourceRegistry registry = make_registry(clip_id, skeleton_id);
    AnimationDecodeCache animation_cache{registry, "Animation"};
    runtime::PropertyStore<CurrentAnimation> current_animations;
    current_animations.set(entity, CurrentAnimation{.clip = clip_id});
    runtime::PropertyStore<AnimationPlaybackConfig> playback_configs;
    playback_configs.set(entity, AnimationPlaybackConfig{.skeleton = skeleton_id, .loop = false});
    runtime::PropertyStore<AnimationClipTime> clip_times;
    runtime::PropertyStore<AnimationPose> poses;
    const std::vector<EntityRef> entities{entity};

    // two_joint_two_keyframe.animation's keyframes sit at t=0.0 and t=1.0;
    // advancing by exactly 1.0 lands precisely on the last keyframe, so the
    // written pose should equal it exactly (no interpolation slop to guard
    // against with EXPECT_NEAR).
    sync_animation_poses(entities,
                         current_animations,
                         playback_configs,
                         clip_times,
                         poses,
                         animation_cache,
                         registry,
                         "Skeleton",
                         1.0);

    ASSERT_TRUE(poses.get(entity).has_value());
    const AnimationPose& pose = poses.get(entity)->get();
    ASSERT_EQ(pose.joint_transforms.size(), 2U);
    EXPECT_FLOAT_EQ(pose.joint_transforms[0].position.x, 10.0F);
    EXPECT_FLOAT_EQ(pose.joint_transforms[0].position.y, 20.0F);
    EXPECT_FLOAT_EQ(pose.joint_transforms[0].position.z, 30.0F);
}

TEST(SyncAnimationPoses, UnresolvedClipLeavesPoseUntouched) {
    const ResourceId clip_id = ResourceId::from_name("animations/pose-sync/clip-4");
    const ResourceId skeleton_id = ResourceId::from_name("skeletons/pose-sync/skeleton-4");
    const EntityRef entity{1};
    const resource::ResourceRegistry registry = make_registry(clip_id, skeleton_id);
    AnimationDecodeCache animation_cache{registry, "Animation"};
    runtime::PropertyStore<CurrentAnimation> current_animations;
    current_animations.set(entity,
                           CurrentAnimation{.clip = ResourceId::from_name("animations/pose-sync/nope")});
    runtime::PropertyStore<AnimationPlaybackConfig> playback_configs;
    playback_configs.set(entity, AnimationPlaybackConfig{.skeleton = skeleton_id, .loop = false});
    runtime::PropertyStore<AnimationClipTime> clip_times;
    runtime::PropertyStore<AnimationPose> poses;
    const std::vector<EntityRef> entities{entity};

    sync_animation_poses(entities,
                         current_animations,
                         playback_configs,
                         clip_times,
                         poses,
                         animation_cache,
                         registry,
                         "Skeleton",
                         0.5);

    EXPECT_FALSE(poses.get(entity).has_value());
    ASSERT_TRUE(clip_times.get(entity).has_value());
    EXPECT_DOUBLE_EQ(clip_times.get(entity)->get().elapsed_seconds, 0.5);
}

TEST(SyncAnimationPoses, UnresolvedSkeletonLeavesPoseUntouched) {
    const ResourceId clip_id = ResourceId::from_name("animations/pose-sync/clip-5");
    const ResourceId skeleton_id = ResourceId::from_name("skeletons/pose-sync/skeleton-5");
    const ResourceId missing_skeleton_id = ResourceId::from_name("skeletons/pose-sync/missing-5");
    const EntityRef entity{1};
    const resource::ResourceRegistry registry = make_registry(clip_id, skeleton_id);
    AnimationDecodeCache animation_cache{registry, "Animation"};
    runtime::PropertyStore<CurrentAnimation> current_animations;
    current_animations.set(entity, CurrentAnimation{.clip = clip_id});
    runtime::PropertyStore<AnimationPlaybackConfig> playback_configs;
    playback_configs.set(entity, AnimationPlaybackConfig{.skeleton = missing_skeleton_id, .loop = false});
    runtime::PropertyStore<AnimationClipTime> clip_times;
    runtime::PropertyStore<AnimationPose> poses;
    const std::vector<EntityRef> entities{entity};

    sync_animation_poses(entities,
                         current_animations,
                         playback_configs,
                         clip_times,
                         poses,
                         animation_cache,
                         registry,
                         "Skeleton",
                         0.25);

    EXPECT_FALSE(poses.get(entity).has_value());
}

TEST(SyncAnimationPoses, MismatchedSkeletonJointCountLeavesPoseUntouched) {
    const ResourceId clip_id = ResourceId::from_name("animations/pose-sync/clip-6");
    const ResourceId mismatched_skeleton_id = ResourceId::from_name("skeletons/pose-sync/mismatched-6");
    const EntityRef entity{1};
    const auto clip_blob_path =
        write_temp_blob("pose_sync_clip_6.blob",
                        pack_single_entry_blob(clip_id, read_fixture("two_joint_two_keyframe.animation")));
    const auto mismatched_blob_path = write_temp_blob(
        "pose_sync_mismatched_skeleton.blob",
        pack_single_entry_blob(mismatched_skeleton_id, read_fixture("three_joint_chain.skeleton")));
    const resource::ResourceRegistry registry{
        {{"Animation", clip_blob_path}, {"Skeleton", mismatched_blob_path}}};
    AnimationDecodeCache animation_cache{registry, "Animation"};
    runtime::PropertyStore<CurrentAnimation> current_animations;
    current_animations.set(entity, CurrentAnimation{.clip = clip_id});
    runtime::PropertyStore<AnimationPlaybackConfig> playback_configs;
    playback_configs.set(entity, AnimationPlaybackConfig{.skeleton = mismatched_skeleton_id, .loop = false});
    runtime::PropertyStore<AnimationClipTime> clip_times;
    runtime::PropertyStore<AnimationPose> poses;
    const std::vector<EntityRef> entities{entity};

    sync_animation_poses(entities,
                         current_animations,
                         playback_configs,
                         clip_times,
                         poses,
                         animation_cache,
                         registry,
                         "Skeleton",
                         0.25);

    EXPECT_FALSE(poses.get(entity).has_value());
}

TEST(SyncAnimationPoses, RepeatedCallsAccumulateElapsedSecondsAcrossCalls) {
    const ResourceId clip_id = ResourceId::from_name("animations/pose-sync/clip-7");
    const ResourceId skeleton_id = ResourceId::from_name("skeletons/pose-sync/skeleton-7");
    const EntityRef entity{1};
    const resource::ResourceRegistry registry = make_registry(clip_id, skeleton_id);
    AnimationDecodeCache animation_cache{registry, "Animation"};
    runtime::PropertyStore<CurrentAnimation> current_animations;
    current_animations.set(entity, CurrentAnimation{.clip = clip_id});
    runtime::PropertyStore<AnimationPlaybackConfig> playback_configs;
    playback_configs.set(entity, AnimationPlaybackConfig{.skeleton = skeleton_id, .loop = false});
    runtime::PropertyStore<AnimationClipTime> clip_times;
    runtime::PropertyStore<AnimationPose> poses;
    const std::vector<EntityRef> entities{entity};

    sync_animation_poses(entities,
                         current_animations,
                         playback_configs,
                         clip_times,
                         poses,
                         animation_cache,
                         registry,
                         "Skeleton",
                         0.5);
    sync_animation_poses(entities,
                         current_animations,
                         playback_configs,
                         clip_times,
                         poses,
                         animation_cache,
                         registry,
                         "Skeleton",
                         0.5);

    ASSERT_TRUE(clip_times.get(entity).has_value());
    EXPECT_DOUBLE_EQ(clip_times.get(entity)->get().elapsed_seconds, 1.0);
    ASSERT_TRUE(poses.get(entity).has_value());
    EXPECT_FLOAT_EQ(poses.get(entity)->get().joint_transforms[0].position.x, 10.0F);
}

} // namespace
} // namespace atlas::render
