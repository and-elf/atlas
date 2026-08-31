#include "atlas/adl/asset_request.hpp"

#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <stdexcept>
#include <string>

namespace atlas::adl {
namespace {

constexpr std::string_view valid_request = R"(
request:
  kind: creature
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf
  overrides:
    - field: fur_color
      value: "#4a3c2f"
      rationale: "unique color for boss variant"

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: idle
      mode: procedural_auto
      loop: true
      duration_seconds: 2.0
    - name: attack_bite
      mode: human_gated
      sync_to: attack_resolution.hit_confirm
      contact_frame_ratio: 0.6

composition:
  intended_role: hostile_creature
  existing_capabilities: [health, auto_attack]
  requires_new_mechanism: false
)";

TEST(ParseAssetRequest, ParsesAFullyValidRequest) {
    const AssetRequest request = parse_asset_request(valid_request);

    EXPECT_EQ(request.kind, AssetRequestKind::Creature);
    EXPECT_EQ(request.name, "enemy_wolf");

    EXPECT_EQ(request.visual.style_ref, "style-guide.yaml#creatures.wolf");
    ASSERT_EQ(request.visual.overrides.size(), 1U);
    EXPECT_EQ(request.visual.overrides[0].field, "fur_color");
    EXPECT_EQ(request.visual.overrides[0].value, "#4a3c2f");
    EXPECT_EQ(request.visual.overrides[0].rationale, "unique color for boss variant");

    EXPECT_EQ(request.rig.type, "quadruped");
    EXPECT_EQ(request.rig.skeleton_template, "quadruped_base");
    ASSERT_EQ(request.rig.animation_set.size(), 2U);

    const AnimationSetEntry& idle = request.rig.animation_set[0];
    EXPECT_EQ(idle.name, "idle");
    EXPECT_EQ(idle.mode, AnimationMode::ProceduralAuto);
    ASSERT_TRUE(idle.loop.has_value());
    EXPECT_TRUE(*idle.loop);
    ASSERT_TRUE(idle.duration_seconds.has_value());
    EXPECT_DOUBLE_EQ(*idle.duration_seconds, 2.0);
    EXPECT_FALSE(idle.sync_to.has_value());
    EXPECT_FALSE(idle.contact_frame_ratio.has_value());

    const AnimationSetEntry& attack = request.rig.animation_set[1];
    EXPECT_EQ(attack.name, "attack_bite");
    EXPECT_EQ(attack.mode, AnimationMode::HumanGated);
    ASSERT_TRUE(attack.sync_to.has_value());
    EXPECT_EQ(*attack.sync_to, "attack_resolution.hit_confirm");
    ASSERT_TRUE(attack.contact_frame_ratio.has_value());
    EXPECT_DOUBLE_EQ(*attack.contact_frame_ratio, 0.6);
    EXPECT_FALSE(attack.loop.has_value());
    EXPECT_FALSE(attack.duration_seconds.has_value());

    EXPECT_EQ(request.composition.intended_role, "hostile_creature");
    ASSERT_EQ(request.composition.existing_capabilities.size(), 2U);
    EXPECT_EQ(request.composition.existing_capabilities[0], "health");
    EXPECT_EQ(request.composition.existing_capabilities[1], "auto_attack");
    EXPECT_FALSE(request.composition.requires_new_mechanism);
    EXPECT_TRUE(request.composition.rationale.empty());
}

TEST(ParseAssetRequest, RejectsMalformedYaml) {
    constexpr std::string_view malformed = "request: [this is not: - valid";

    EXPECT_THROW({ (void)parse_asset_request(malformed); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsNonMappingDocumentRoot) {
    constexpr std::string_view sequence_root = "- just\n- a\n- sequence\n";

    EXPECT_THROW({ (void)parse_asset_request(sequence_root); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsMissingRequestBlock) {
    constexpr std::string_view no_request = "unrelated: value\n";

    EXPECT_THROW({ (void)parse_asset_request(no_request); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsRootWithUnrecognizedField) {
    const std::string with_extra = std::string(valid_request) + "\nextra: field\n";

    EXPECT_THROW({ (void)parse_asset_request(with_extra); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsRequestBlockWithUnrecognizedField) {
    constexpr std::string_view unknown_field = R"(
request:
  kind: creature
  name: enemy_wolf
  tags: [boss]

visual:
  style_ref: style-guide.yaml#creatures.wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: idle
      mode: procedural_auto
      loop: true
      duration_seconds: 2.0

composition:
  intended_role: hostile_creature
  requires_new_mechanism: false
)";

    EXPECT_THROW({ (void)parse_asset_request(unknown_field); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsUnrecognizedKind) {
    constexpr std::string_view bad_kind = R"(
request:
  kind: dragon
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: idle
      mode: procedural_auto
      loop: true
      duration_seconds: 2.0

composition:
  intended_role: hostile_creature
  requires_new_mechanism: false
)";

    EXPECT_THROW({ (void)parse_asset_request(bad_kind); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsEmptyName) {
    constexpr std::string_view empty_name = R"(
request:
  kind: creature
  name: ""

visual:
  style_ref: style-guide.yaml#creatures.wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: idle
      mode: procedural_auto
      loop: true
      duration_seconds: 2.0

composition:
  intended_role: hostile_creature
  requires_new_mechanism: false
)";

    EXPECT_THROW({ (void)parse_asset_request(empty_name); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsMissingVisualBlock) {
    constexpr std::string_view no_visual = R"(
request:
  kind: creature
  name: enemy_wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: idle
      mode: procedural_auto
      loop: true
      duration_seconds: 2.0

composition:
  intended_role: hostile_creature
  requires_new_mechanism: false
)";

    EXPECT_THROW({ (void)parse_asset_request(no_visual); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsVisualOverrideMissingRationale) {
    constexpr std::string_view missing_rationale = R"(
request:
  kind: creature
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf
  overrides:
    - field: fur_color
      value: "#4a3c2f"

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: idle
      mode: procedural_auto
      loop: true
      duration_seconds: 2.0

composition:
  intended_role: hostile_creature
  requires_new_mechanism: false
)";

    EXPECT_THROW({ (void)parse_asset_request(missing_rationale); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsMissingRigBlock) {
    constexpr std::string_view no_rig = R"(
request:
  kind: creature
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf

composition:
  intended_role: hostile_creature
  requires_new_mechanism: false
)";

    EXPECT_THROW({ (void)parse_asset_request(no_rig); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsMissingAnimationSet) {
    constexpr std::string_view no_animation_set = R"(
request:
  kind: creature
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base

composition:
  intended_role: hostile_creature
  requires_new_mechanism: false
)";

    EXPECT_THROW({ (void)parse_asset_request(no_animation_set); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsUnrecognizedAnimationMode) {
    constexpr std::string_view bad_mode = R"(
request:
  kind: creature
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: idle
      mode: hand_animated
      loop: true
      duration_seconds: 2.0

composition:
  intended_role: hostile_creature
  requires_new_mechanism: false
)";

    EXPECT_THROW({ (void)parse_asset_request(bad_mode); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsProceduralAutoMissingLoop) {
    constexpr std::string_view missing_loop = R"(
request:
  kind: creature
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: idle
      mode: procedural_auto
      duration_seconds: 2.0

composition:
  intended_role: hostile_creature
  requires_new_mechanism: false
)";

    EXPECT_THROW({ (void)parse_asset_request(missing_loop); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsProceduralAutoMissingDurationSeconds) {
    constexpr std::string_view missing_duration = R"(
request:
  kind: creature
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: idle
      mode: procedural_auto
      loop: true

composition:
  intended_role: hostile_creature
  requires_new_mechanism: false
)";

    EXPECT_THROW({ (void)parse_asset_request(missing_duration); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsHumanGatedMissingSyncTo) {
    constexpr std::string_view missing_sync_to = R"(
request:
  kind: creature
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: attack_bite
      mode: human_gated
      contact_frame_ratio: 0.6

composition:
  intended_role: hostile_creature
  requires_new_mechanism: false
)";

    EXPECT_THROW({ (void)parse_asset_request(missing_sync_to); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsHumanGatedMissingContactFrameRatio) {
    constexpr std::string_view missing_ratio = R"(
request:
  kind: creature
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: attack_bite
      mode: human_gated
      sync_to: attack_resolution.hit_confirm

composition:
  intended_role: hostile_creature
  requires_new_mechanism: false
)";

    EXPECT_THROW({ (void)parse_asset_request(missing_ratio); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsContactFrameRatioBelowZero) {
    constexpr std::string_view negative_ratio = R"(
request:
  kind: creature
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: attack_bite
      mode: human_gated
      sync_to: attack_resolution.hit_confirm
      contact_frame_ratio: -0.1

composition:
  intended_role: hostile_creature
  requires_new_mechanism: false
)";

    EXPECT_THROW({ (void)parse_asset_request(negative_ratio); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsContactFrameRatioAboveOne) {
    constexpr std::string_view over_one_ratio = R"(
request:
  kind: creature
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: attack_bite
      mode: human_gated
      sync_to: attack_resolution.hit_confirm
      contact_frame_ratio: 1.1

composition:
  intended_role: hostile_creature
  requires_new_mechanism: false
)";

    EXPECT_THROW({ (void)parse_asset_request(over_one_ratio); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsMissingCompositionBlock) {
    constexpr std::string_view no_composition = R"(
request:
  kind: creature
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: idle
      mode: procedural_auto
      loop: true
      duration_seconds: 2.0
)";

    EXPECT_THROW({ (void)parse_asset_request(no_composition); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsMissingIntendedRole) {
    constexpr std::string_view missing_intended_role = R"(
request:
  kind: creature
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: idle
      mode: procedural_auto
      loop: true
      duration_seconds: 2.0

composition:
  requires_new_mechanism: false
)";

    EXPECT_THROW({ (void)parse_asset_request(missing_intended_role); }, std::invalid_argument);
}

TEST(ParseAssetRequest, RejectsRequiresNewMechanismTrueWithoutRationale) {
    constexpr std::string_view missing_rationale = R"(
request:
  kind: creature
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: idle
      mode: procedural_auto
      loop: true
      duration_seconds: 2.0

composition:
  intended_role: hostile_creature
  requires_new_mechanism: true
)";

    EXPECT_THROW({ (void)parse_asset_request(missing_rationale); }, std::invalid_argument);
}

TEST(ParseAssetRequest, AcceptsRequiresNewMechanismTrueWithRationale) {
    constexpr std::string_view with_rationale = R"(
request:
  kind: creature
  name: enemy_wolf

visual:
  style_ref: style-guide.yaml#creatures.wolf

rig:
  type: quadruped
  skeleton_template: quadruped_base
  animation_set:
    - name: idle
      mode: procedural_auto
      loop: true
      duration_seconds: 2.0

composition:
  intended_role: hostile_creature
  requires_new_mechanism: true
  rationale: "no existing capability composes a burrow-and-ambush behavior"
)";

    const AssetRequest request = parse_asset_request(with_rationale);
    EXPECT_TRUE(request.composition.requires_new_mechanism);
    EXPECT_EQ(request.composition.rationale, "no existing capability composes a burrow-and-ambush behavior");
}

TEST(ParseAssetRequest, ValidatesTheRealDemoExampleFile) {
    std::ifstream input(std::string(ATLAS_ADL_DEMO_REQUESTS_DIR) + "/enemy_wolf.request.yaml");
    ASSERT_TRUE(input.is_open());
    std::ostringstream buffer;
    buffer << input.rdbuf();

    const AssetRequest request = parse_asset_request(buffer.str());
    EXPECT_EQ(request.kind, AssetRequestKind::Creature);
    EXPECT_EQ(request.name, "enemy_wolf");
}

} // namespace
} // namespace atlas::adl
