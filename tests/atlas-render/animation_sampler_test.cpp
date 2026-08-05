#include "atlas/render/animation_sampler.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace atlas::render {
namespace {

Transform make_transform(float x, float rotation_y, float rotation_w, float scale) {
    return Transform{
        .position = {x, 0.0F, 0.0F},
        .rotation = {0.0F, rotation_y, 0.0F, rotation_w},
        .scale = {scale, scale, scale},
    };
}

SkeletonAsset make_two_joint_skeleton() {
    SkeletonAsset skeleton;
    skeleton.joints.push_back(Joint{.parent_index = no_parent_joint, .bind_pose = Transform{}});
    skeleton.joints.push_back(Joint{.parent_index = 0, .bind_pose = Transform{}});
    return skeleton;
}

// Two joints, three keyframes at t=0, t=1, t=2 - joint 0's x position moves
// linearly 0 -> 10 -> 20 across the whole clip (t=1's midpoint value, 10, is
// exact and easy to assert against without floating-point interpolation
// slop), joint 1 stays fixed at x=5 throughout so tests can distinguish
// "joint 0 changed, joint 1 didn't" unambiguously.
DecodedAnimation make_three_keyframe_animation() {
    DecodedAnimation animation;
    animation.joint_count = 2;
    animation.keyframes.push_back(AnimationKeyframe{
        .time = 0.0F,
        .joint_transforms = {make_transform(0.0F, 0.0F, 1.0F, 1.0F), make_transform(5.0F, 0.0F, 1.0F, 1.0F)},
    });
    animation.keyframes.push_back(AnimationKeyframe{
        .time = 1.0F,
        .joint_transforms = {make_transform(10.0F, 0.0F, 1.0F, 1.0F), make_transform(5.0F, 0.0F, 1.0F, 1.0F)},
    });
    animation.keyframes.push_back(AnimationKeyframe{
        .time = 2.0F,
        .joint_transforms = {make_transform(20.0F, 0.0F, 1.0F, 2.0F), make_transform(5.0F, 0.0F, 1.0F, 1.0F)},
    });
    return animation;
}

TEST(SampleAnimationPose, MismatchedJointCountReturnsNullopt) {
    DecodedAnimation animation = make_three_keyframe_animation();
    animation.joint_count = 3; // skeleton below only has 2 joints
    const SkeletonAsset skeleton = make_two_joint_skeleton();

    const std::optional<AnimationPose> pose = sample_animation_pose(animation, skeleton, 0.0, false);

    EXPECT_FALSE(pose.has_value());
}

TEST(SampleAnimationPose, EmptyKeyframesReturnsNullopt) {
    DecodedAnimation animation;
    animation.joint_count = 2;
    const SkeletonAsset skeleton = make_two_joint_skeleton();

    const std::optional<AnimationPose> pose = sample_animation_pose(animation, skeleton, 0.0, false);

    EXPECT_FALSE(pose.has_value());
}

TEST(SampleAnimationPose, SingleKeyframeReturnsThatKeyframesPoseRegardlessOfElapsedSeconds) {
    DecodedAnimation animation;
    animation.joint_count = 2;
    animation.keyframes.push_back(AnimationKeyframe{
        .time = 0.0F,
        .joint_transforms = {make_transform(3.0F, 0.0F, 1.0F, 1.0F), make_transform(7.0F, 0.0F, 1.0F, 1.0F)},
    });
    const SkeletonAsset skeleton = make_two_joint_skeleton();

    const std::optional<AnimationPose> pose_at_zero = sample_animation_pose(animation, skeleton, 0.0, false);
    const std::optional<AnimationPose> pose_far_out = sample_animation_pose(animation, skeleton, 500.0, true);

    ASSERT_TRUE(pose_at_zero.has_value());
    ASSERT_TRUE(pose_far_out.has_value());
    ASSERT_EQ(pose_at_zero->joint_transforms.size(), 2U);
    EXPECT_FLOAT_EQ(pose_at_zero->joint_transforms[0].position.x, 3.0F);
    EXPECT_FLOAT_EQ(pose_at_zero->joint_transforms[1].position.x, 7.0F);
    EXPECT_FLOAT_EQ(pose_far_out->joint_transforms[0].position.x, 3.0F);
    EXPECT_FLOAT_EQ(pose_far_out->joint_transforms[1].position.x, 7.0F);
}

TEST(SampleAnimationPose, NonLoopingClampsNegativeElapsedSecondsToTheFirstKeyframe) {
    const DecodedAnimation animation = make_three_keyframe_animation();
    const SkeletonAsset skeleton = make_two_joint_skeleton();

    const std::optional<AnimationPose> pose = sample_animation_pose(animation, skeleton, -5.0, false);

    ASSERT_TRUE(pose.has_value());
    EXPECT_FLOAT_EQ(pose->joint_transforms[0].position.x, 0.0F);
}

TEST(SampleAnimationPose, NonLoopingClampsElapsedSecondsPastTheEndToTheLastKeyframe) {
    const DecodedAnimation animation = make_three_keyframe_animation();
    const SkeletonAsset skeleton = make_two_joint_skeleton();

    const std::optional<AnimationPose> pose = sample_animation_pose(animation, skeleton, 100.0, false);

    ASSERT_TRUE(pose.has_value());
    EXPECT_FLOAT_EQ(pose->joint_transforms[0].position.x, 20.0F);
    EXPECT_FLOAT_EQ(pose->joint_transforms[0].scale.x, 2.0F);
}

TEST(SampleAnimationPose, ExactKeyframeTimeReturnsThatKeyframesPoseWithoutInterpolationDrift) {
    const DecodedAnimation animation = make_three_keyframe_animation();
    const SkeletonAsset skeleton = make_two_joint_skeleton();

    const std::optional<AnimationPose> pose = sample_animation_pose(animation, skeleton, 1.0, false);

    ASSERT_TRUE(pose.has_value());
    EXPECT_FLOAT_EQ(pose->joint_transforms[0].position.x, 10.0F);
    EXPECT_FLOAT_EQ(pose->joint_transforms[1].position.x, 5.0F);
}

TEST(SampleAnimationPose, InterpolatesLinearlyBetweenTheTwoBracketingKeyframes) {
    const DecodedAnimation animation = make_three_keyframe_animation();
    const SkeletonAsset skeleton = make_two_joint_skeleton();

    const std::optional<AnimationPose> pose = sample_animation_pose(animation, skeleton, 1.5, false);

    // Halfway through the [1.0, 2.0] segment: joint 0's x lerps 10 -> 20,
    // giving 15; joint 1 stays fixed at 5 across every keyframe.
    ASSERT_TRUE(pose.has_value());
    EXPECT_FLOAT_EQ(pose->joint_transforms[0].position.x, 15.0F);
    EXPECT_FLOAT_EQ(pose->joint_transforms[1].position.x, 5.0F);
}

TEST(SampleAnimationPose, LocatesTheCorrectBracketWhenThreeOrMoreKeyframesArePresent) {
    const DecodedAnimation animation = make_three_keyframe_animation();
    const SkeletonAsset skeleton = make_two_joint_skeleton();

    // Falls in the first segment [0.0, 1.0], not the second - a regression
    // guard for a bracket-search that only ever looks at the last segment.
    const std::optional<AnimationPose> pose = sample_animation_pose(animation, skeleton, 0.5, false);

    ASSERT_TRUE(pose.has_value());
    EXPECT_FLOAT_EQ(pose->joint_transforms[0].position.x, 5.0F);
}

TEST(SampleAnimationPose, LoopingWrapsElapsedSecondsModuloTheClipLength) {
    const DecodedAnimation animation = make_three_keyframe_animation();
    const SkeletonAsset skeleton = make_two_joint_skeleton();

    // Clip length is 2.0 (last keyframe's time); 2.5 wraps to 0.5, the same
    // point NonLoopingClamps... would reach without wrapping.
    const std::optional<AnimationPose> looped = sample_animation_pose(animation, skeleton, 2.5, true);
    const std::optional<AnimationPose> unwrapped = sample_animation_pose(animation, skeleton, 0.5, false);

    ASSERT_TRUE(looped.has_value());
    ASSERT_TRUE(unwrapped.has_value());
    EXPECT_FLOAT_EQ(looped->joint_transforms[0].position.x, unwrapped->joint_transforms[0].position.x);
}

TEST(SampleAnimationPose, LoopingWithNegativeElapsedSecondsStillWrapsIntoRange) {
    const DecodedAnimation animation = make_three_keyframe_animation();
    const SkeletonAsset skeleton = make_two_joint_skeleton();

    // -0.5 wraps to 1.5 (2.0 - 0.5) within a 2.0-length loop.
    const std::optional<AnimationPose> looped = sample_animation_pose(animation, skeleton, -0.5, true);
    const std::optional<AnimationPose> reference = sample_animation_pose(animation, skeleton, 1.5, false);

    ASSERT_TRUE(looped.has_value());
    ASSERT_TRUE(reference.has_value());
    EXPECT_FLOAT_EQ(looped->joint_transforms[0].position.x, reference->joint_transforms[0].position.x);
}

} // namespace
} // namespace atlas::render
