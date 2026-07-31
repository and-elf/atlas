#include "atlas/refl/manifest.hpp"
#include "atlas/refl/reflection_writer.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace atlas::refl {
namespace {

constexpr std::string_view health_manifest_yaml = R"(
capability:
  name: health
depends_on: [entity]

properties:
  Health:
    current: int32
    maximum: int32

requests:
  ApplyDamage:
    target: EntityRef
    amount: int32

events:
  HealthChanged:
    target: EntityRef
    new_current: int32
)";

TEST(GenerateReflectionMetadata, ReproducesTheHealthWorkedExample) {
    const Manifest manifest = parse_manifest(health_manifest_yaml);

    const std::string output =
        generate_reflection_metadata(manifest, "health.capability.reflection.hpp", "health.capability.yaml");

    EXPECT_NE(output.find("// GENERATED — health.capability.reflection.hpp"), std::string::npos);
    EXPECT_NE(output.find("Source: health.capability.yaml — do not hand-edit."), std::string::npos);
    EXPECT_NE(output.find("#include \"atlas/refl/field_metadata.hpp\""), std::string::npos);
    EXPECT_NE(output.find("namespace atlas::refl::health {"), std::string::npos);

    EXPECT_NE(output.find("std::array<atlas::refl::FieldMetadata, 2> kHealthFields{"), std::string::npos);
    EXPECT_NE(output.find(R"({"current", "std::int32_t"},)"), std::string::npos);
    EXPECT_NE(output.find(R"({"maximum", "std::int32_t"},)"), std::string::npos);

    EXPECT_NE(output.find("std::array<atlas::refl::FieldMetadata, 2> kApplyDamageFields{"),
              std::string::npos);
    EXPECT_NE(output.find(R"({"target", "atlas::EntityRef"},)"), std::string::npos);
    EXPECT_NE(output.find(R"({"amount", "std::int32_t"},)"), std::string::npos);

    EXPECT_NE(output.find("std::array<atlas::refl::FieldMetadata, 2> kHealthChangedFields{"),
              std::string::npos);
    EXPECT_NE(output.find(R"({"new_current", "std::int32_t"},)"), std::string::npos);

    // No composition constant anywhere - none of health's structs declare one.
    EXPECT_EQ(output.find("Composition"), std::string::npos);

    // Struct order must follow manifest declaration order (properties, then
    // requests, then events).
    const auto health_pos = output.find("kHealthFields");
    const auto apply_damage_pos = output.find("kApplyDamageFields");
    const auto health_changed_pos = output.find("kHealthChangedFields");
    ASSERT_NE(health_pos, std::string::npos);
    ASSERT_NE(apply_damage_pos, std::string::npos);
    ASSERT_NE(health_changed_pos, std::string::npos);
    EXPECT_LT(health_pos, apply_damage_pos);
    EXPECT_LT(apply_damage_pos, health_changed_pos);
}

TEST(GenerateReflectionMetadata, ComposedPropertyEmitsACompositionConstant) {
    constexpr std::string_view yaml = R"(
capability:
  name: armor
properties:
  Armor:
    composition: Additive
    base: int32
)";
    const Manifest manifest = parse_manifest(yaml);

    const std::string result =
        generate_reflection_metadata(manifest, "armor.capability.reflection.hpp", "armor.capability.yaml");

    EXPECT_NE(result.find("std::array<atlas::refl::FieldMetadata, 1> kArmorFields{"), std::string::npos);
    EXPECT_NE(result.find(R"({"base", "std::int32_t"},)"), std::string::npos);
    EXPECT_NE(result.find(
                  "inline constexpr std::string_view kArmorComposition = \"atlas::Composition::Additive\";"),
              std::string::npos);

    // The composition constant must appear after the fields array, matching
    // the order fields/then-composition is declared in this generator.
    const auto fields_pos = result.find("kArmorFields");
    const auto composition_pos = result.find("kArmorComposition");
    ASSERT_NE(fields_pos, std::string::npos);
    ASSERT_NE(composition_pos, std::string::npos);
    EXPECT_LT(fields_pos, composition_pos);
}

TEST(GenerateReflectionMetadata, NonComposedPropertyEmitsNoCompositionConstant) {
    const Manifest manifest = parse_manifest(health_manifest_yaml);

    const std::string result =
        generate_reflection_metadata(manifest, "health.capability.reflection.hpp", "health.capability.yaml");

    EXPECT_EQ(result.find("Composition"), std::string::npos);
}

TEST(GenerateReflectionMetadata, EmptyManifestProducesAnEmptyButValidNamespaceBlock) {
    constexpr std::string_view yaml = R"(
capability:
  name: nothing_yet
)";
    const Manifest manifest = parse_manifest(yaml);

    const std::string result = generate_reflection_metadata(
        manifest, "nothing_yet.capability.reflection.hpp", "nothing_yet.capability.yaml");

    EXPECT_NE(result.find("namespace atlas::refl::nothing_yet {"), std::string::npos);
    EXPECT_EQ(result.find("std::array"), std::string::npos);
}

TEST(GenerateReflectionMetadata, RejectsAFieldTypeMapFieldTypeDoesNotRecognize) {
    // parse_manifest already validates every declared field type, so this
    // path is unreachable through the parser - construct a Manifest by hand
    // to prove generate_reflection_metadata's own defensive check (it
    // doesn't just trust an already-validated caller) actually works.
    Manifest manifest;
    manifest.capability_name = "bad";
    manifest.properties.push_back(StructDecl{
        .name = "Bad",
        .fields = {Field{.name = "thing", .type = "not_a_real_type"}},
        .composition = std::nullopt,
    });

    EXPECT_THROW(
        (void)generate_reflection_metadata(manifest, "bad.capability.reflection.hpp", "bad.capability.yaml"),
        std::invalid_argument);
}

TEST(GenerateReflectionMetadata, RejectsACompositionStrategyMapCompositionStrategyDoesNotRecognize) {
    // Same defensive-check reasoning as above, for the composition strategy
    // token this time.
    Manifest manifest;
    manifest.capability_name = "bad";
    manifest.properties.push_back(StructDecl{
        .name = "Bad",
        .fields = {Field{.name = "base", .type = "int32"}},
        .composition = "not_a_real_strategy",
    });

    EXPECT_THROW(
        (void)generate_reflection_metadata(manifest, "bad.capability.reflection.hpp", "bad.capability.yaml"),
        std::invalid_argument);
}

} // namespace
} // namespace atlas::refl
