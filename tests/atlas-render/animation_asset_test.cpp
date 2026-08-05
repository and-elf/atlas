#include "atlas/render/animation_asset.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <vector>

namespace atlas::render {
namespace {

// Set by CMakeLists.txt to the absolute path of tests/atlas-render/fixtures/
// - real fixture files on disk, not inline byte arrays, mirroring
// skeleton_asset_test.cpp/mesh_asset_test.cpp's own read_fixture helper and
// rationale.
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

// Field-by-field, unrolled rather than looped, mirroring
// skeleton_asset_test.cpp's own WellFormedThreeJointChainDecodes...
// rationale: a loop over EXPECT_EQ/EXPECT_FLOAT_EQ macros here pushes
// TestBody's cognitive complexity past this project's clang-tidy threshold
// for no real gain, since two_joint_two_keyframe.animation's shape is fixed
// and known. two_joint_two_keyframe.animation was hand-constructed (a
// one-off Python struct.pack script, not checked in as a maintained tool -
// the same "document how in a comment" latitude issue #228 itself took for
// three_joint_chain.skeleton): 2 joints, 2 keyframes - keyframe 0 (time 0.0,
// both joints at identity-ish rest positions), keyframe 1 (time 1.0, joint 0
// carrying a distinct position, a 90-degree-about-Y rotation
// (qy = qw = sqrt(2)/2), and a non-unit scale; joint 1 only translated).
TEST(DecodeAnimation, WellFormedTwoJointTwoKeyframeClipDecodesToExactlyTheExpectedKeyframes) {
    const std::vector<std::byte> bytes = read_fixture("two_joint_two_keyframe.animation");

    const std::optional<DecodedAnimation> animation = decode_animation(bytes);

    ASSERT_TRUE(animation.has_value());
    EXPECT_EQ(animation->joint_count, 2U);
    ASSERT_EQ(animation->keyframes.size(), 2U);

    const AnimationKeyframe& keyframe0 = animation->keyframes[0];
    EXPECT_FLOAT_EQ(keyframe0.time, 0.0F);
    ASSERT_EQ(keyframe0.joint_transforms.size(), 2U);
    EXPECT_FLOAT_EQ(keyframe0.joint_transforms[0].position.x, 0.0F);
    EXPECT_FLOAT_EQ(keyframe0.joint_transforms[0].rotation.w, 1.0F);
    EXPECT_FLOAT_EQ(keyframe0.joint_transforms[0].scale.x, 1.0F);
    EXPECT_FLOAT_EQ(keyframe0.joint_transforms[1].position.x, 1.0F);
    EXPECT_FLOAT_EQ(keyframe0.joint_transforms[1].position.y, 2.0F);
    EXPECT_FLOAT_EQ(keyframe0.joint_transforms[1].position.z, 3.0F);

    const AnimationKeyframe& keyframe1 = animation->keyframes[1];
    EXPECT_FLOAT_EQ(keyframe1.time, 1.0F);
    ASSERT_EQ(keyframe1.joint_transforms.size(), 2U);
    EXPECT_FLOAT_EQ(keyframe1.joint_transforms[0].position.x, 10.0F);
    EXPECT_FLOAT_EQ(keyframe1.joint_transforms[0].position.y, 20.0F);
    EXPECT_FLOAT_EQ(keyframe1.joint_transforms[0].position.z, 30.0F);
    EXPECT_FLOAT_EQ(keyframe1.joint_transforms[0].rotation.y, 0.70710678F);
    EXPECT_FLOAT_EQ(keyframe1.joint_transforms[0].rotation.w, 0.70710678F);
    EXPECT_FLOAT_EQ(keyframe1.joint_transforms[0].scale.x, 2.0F);
    EXPECT_FLOAT_EQ(keyframe1.joint_transforms[0].scale.z, 2.0F);
    EXPECT_FLOAT_EQ(keyframe1.joint_transforms[1].position.x, 11.0F);
    EXPECT_FLOAT_EQ(keyframe1.joint_transforms[1].position.y, 22.0F);
    EXPECT_FLOAT_EQ(keyframe1.joint_transforms[1].position.z, 33.0F);
}

TEST(DecodeAnimation, RepeatedDecodesOfTheSameBytesProduceBitIdenticalOutput) {
    const std::vector<std::byte> bytes = read_fixture("two_joint_two_keyframe.animation");

    const std::optional<DecodedAnimation> first = decode_animation(bytes);
    const std::optional<DecodedAnimation> second = decode_animation(bytes);

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_EQ(first->keyframes.size(), second->keyframes.size());
    EXPECT_EQ(first->keyframes[1].joint_transforms[0].position.x,
              second->keyframes[1].joint_transforms[0].position.x);
    EXPECT_EQ(first->keyframes[1].joint_transforms[0].rotation.y,
              second->keyframes[1].joint_transforms[0].rotation.y);
}

TEST(DecodeAnimation, ZeroJointAndKeyframeCountsDecodeToAnEmptyAnimation) {
    // Header-only bytes declaring zero joints and zero keyframes is
    // well-formed per the format (animation_asset.hpp): an empty clip, not
    // a malformed one - mirroring decode_mesh's own
    // ZeroVertexAndIndexCountsDecodeToAnEmptyMesh and decode_skeleton's own
    // ZeroJointCountDecodesToAnEmptySkeleton.
    const std::vector<std::byte> bytes(8, std::byte{0});

    const std::optional<DecodedAnimation> animation = decode_animation(bytes);

    ASSERT_TRUE(animation.has_value());
    EXPECT_EQ(animation->joint_count, 0U);
    EXPECT_TRUE(animation->keyframes.empty());
}

TEST(DecodeAnimation, NonZeroJointCountWithZeroKeyframeCountDecodesToAnEmptyKeyframeList) {
    // joint_count declared non-zero, but keyframe_count is zero: still
    // well-formed (there is simply nothing to sample), mirroring the same
    // "each count is independently allowed to be zero" stance the header
    // documents.
    std::vector<std::byte> bytes(8, std::byte{0});
    const std::uint32_t joint_count = 3;
    std::memcpy(bytes.data(), &joint_count, sizeof(joint_count));

    const std::optional<DecodedAnimation> animation = decode_animation(bytes);

    ASSERT_TRUE(animation.has_value());
    EXPECT_EQ(animation->joint_count, 3U);
    EXPECT_TRUE(animation->keyframes.empty());
}

TEST(DecodeAnimation, EmptyInputFailsToDecodeRatherThanReadingOutOfBounds) {
    const std::optional<DecodedAnimation> animation = decode_animation(std::span<const std::byte>{});

    EXPECT_FALSE(animation.has_value());
}

TEST(DecodeAnimation, TruncatedHeaderFailsToDecode) {
    const std::vector<std::byte> bytes = read_fixture("truncated_header.animation");

    const std::optional<DecodedAnimation> animation = decode_animation(bytes);

    EXPECT_FALSE(animation.has_value());
}

TEST(DecodeAnimation, TruncatedKeyframeDataFailsToDecode) {
    const std::vector<std::byte> bytes = read_fixture("truncated_keyframe_data.animation");

    const std::optional<DecodedAnimation> animation = decode_animation(bytes);

    EXPECT_FALSE(animation.has_value());
}

// A declared joint_count/keyframe_count pair whose product would overflow
// std::size_t if multiplied naively (joint_count * 40 * keyframe_count),
// rather than checked via the overflow-safe division guard
// decode_texture's own width * height check documents - both counts are
// individually plausible (well within u32 range) but chosen so their
// combined per-element byte size overflows a 64-bit std::size_t, which must
// be rejected as malformed rather than wrapping around into a small,
// incorrectly "valid"-looking `needed` byte count that this tiny input
// would then spuriously satisfy.
TEST(DecodeAnimation, OverflowingDeclaredSizeFailsToDecodeRatherThanWrappingAround) {
    std::vector<std::byte> bytes(8, std::byte{0});
    const std::uint32_t joint_count = 0xFFFFFFFFU;
    const std::uint32_t keyframe_count = 0xFFFFFFFFU;
    std::memcpy(bytes.data(), &joint_count, sizeof(joint_count));
    std::memcpy(bytes.data() + sizeof(joint_count), &keyframe_count, sizeof(keyframe_count));

    const std::optional<DecodedAnimation> animation = decode_animation(bytes);

    EXPECT_FALSE(animation.has_value());
}

} // namespace
} // namespace atlas::render
