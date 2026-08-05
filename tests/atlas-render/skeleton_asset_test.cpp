#include "atlas/render/skeleton_asset.hpp"

#include <cstddef>
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
// mesh_asset_test.cpp's own read_fixture helper and rationale.
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
// mesh_asset_test.cpp's own DecodeMesh.WellFormedTriangleDecodesToExactly...
// rationale: a loop over EXPECT_EQ/EXPECT_FLOAT_EQ macros here pushes
// TestBody's cognitive complexity past this project's clang-tidy threshold
// for no real gain, since three_joint_chain.skeleton's joint count is fixed
// and known. three_joint_chain.skeleton was hand-constructed (a one-off
// Python struct.pack script, not checked in as a maintained tool - the same
// "document how in a comment" latitude issue #228 itself calls for): root
// (joint 0, parent kNoParentJoint), child of root (joint 1, parent 0),
// grandchild (joint 2, parent 1), each with distinct position/scale values
// and joint 2 additionally carrying a non-identity rotation
// (90 degrees about Y, qy = qw = sqrt(2)/2).
TEST(DecodeSkeleton, WellFormedThreeJointChainDecodesToExactlyTheExpectedHierarchyAndBindPoses) {
    const std::vector<std::byte> bytes = read_fixture("three_joint_chain.skeleton");

    const std::optional<SkeletonAsset> skeleton = decode_skeleton(bytes);

    ASSERT_TRUE(skeleton.has_value());
    ASSERT_EQ(skeleton->joints.size(), 3U);

    EXPECT_EQ(skeleton->joints[0].parent_index, kNoParentJoint);
    EXPECT_FLOAT_EQ(skeleton->joints[0].bind_pose.position.x, 0.0F);
    EXPECT_FLOAT_EQ(skeleton->joints[0].bind_pose.position.y, 0.0F);
    EXPECT_FLOAT_EQ(skeleton->joints[0].bind_pose.position.z, 0.0F);
    EXPECT_FLOAT_EQ(skeleton->joints[0].bind_pose.rotation.w, 1.0F);
    EXPECT_FLOAT_EQ(skeleton->joints[0].bind_pose.scale.x, 1.0F);

    EXPECT_EQ(skeleton->joints[1].parent_index, 0U);
    EXPECT_FLOAT_EQ(skeleton->joints[1].bind_pose.position.x, 1.0F);
    EXPECT_FLOAT_EQ(skeleton->joints[1].bind_pose.position.y, 2.0F);
    EXPECT_FLOAT_EQ(skeleton->joints[1].bind_pose.position.z, 3.0F);
    EXPECT_FLOAT_EQ(skeleton->joints[1].bind_pose.scale.x, 1.0F);

    EXPECT_EQ(skeleton->joints[2].parent_index, 1U);
    EXPECT_FLOAT_EQ(skeleton->joints[2].bind_pose.position.x, 4.0F);
    EXPECT_FLOAT_EQ(skeleton->joints[2].bind_pose.position.y, 5.0F);
    EXPECT_FLOAT_EQ(skeleton->joints[2].bind_pose.position.z, 6.0F);
    EXPECT_FLOAT_EQ(skeleton->joints[2].bind_pose.rotation.y, 0.70710678F);
    EXPECT_FLOAT_EQ(skeleton->joints[2].bind_pose.rotation.w, 0.70710678F);
    EXPECT_FLOAT_EQ(skeleton->joints[2].bind_pose.scale.x, 2.0F);
    EXPECT_FLOAT_EQ(skeleton->joints[2].bind_pose.scale.z, 2.0F);
}

TEST(DecodeSkeleton, RepeatedDecodesOfTheSameBytesProduceBitIdenticalOutput) {
    const std::vector<std::byte> bytes = read_fixture("three_joint_chain.skeleton");

    const std::optional<SkeletonAsset> first = decode_skeleton(bytes);
    const std::optional<SkeletonAsset> second = decode_skeleton(bytes);

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_EQ(first->joints.size(), second->joints.size());
    EXPECT_EQ(first->joints[1].parent_index, second->joints[1].parent_index);
    EXPECT_EQ(first->joints[2].bind_pose.position.x, second->joints[2].bind_pose.position.x);
    EXPECT_EQ(first->joints[2].bind_pose.rotation.y, second->joints[2].bind_pose.rotation.y);
}

TEST(DecodeSkeleton, ZeroJointCountDecodesToAnEmptySkeleton) {
    // Header-only bytes declaring zero joints is well-formed per the format
    // (skeleton_asset.hpp): an empty skeleton, not a malformed one -
    // mirroring decode_mesh's own ZeroVertexAndIndexCountsDecodeToAnEmptyMesh.
    const std::vector<std::byte> bytes(4, std::byte{0});

    const std::optional<SkeletonAsset> skeleton = decode_skeleton(bytes);

    ASSERT_TRUE(skeleton.has_value());
    EXPECT_TRUE(skeleton->joints.empty());
}

TEST(DecodeSkeleton, EmptyInputFailsToDecodeRatherThanReadingOutOfBounds) {
    const std::optional<SkeletonAsset> skeleton = decode_skeleton(std::span<const std::byte>{});

    EXPECT_FALSE(skeleton.has_value());
}

TEST(DecodeSkeleton, TruncatedHeaderFailsToDecode) {
    const std::vector<std::byte> bytes = read_fixture("truncated_header.skeleton");

    const std::optional<SkeletonAsset> skeleton = decode_skeleton(bytes);

    EXPECT_FALSE(skeleton.has_value());
}

TEST(DecodeSkeleton, TruncatedJointDataFailsToDecode) {
    const std::vector<std::byte> bytes = read_fixture("truncated_joint_data.skeleton");

    const std::optional<SkeletonAsset> skeleton = decode_skeleton(bytes);

    EXPECT_FALSE(skeleton.has_value());
}

// invalid_parent_index.skeleton is not truncated - every byte the header
// declares is present - but joint 1's parent_index (2) is neither
// kNoParentJoint nor strictly less than 1, a forward reference the
// hierarchy invariant must reject rather than trust.
TEST(DecodeSkeleton, ParentIndexNotPrecedingItsOwnJointFailsToDecode) {
    const std::vector<std::byte> bytes = read_fixture("invalid_parent_index.skeleton");

    const std::optional<SkeletonAsset> skeleton = decode_skeleton(bytes);

    EXPECT_FALSE(skeleton.has_value());
}

} // namespace
} // namespace atlas::render
