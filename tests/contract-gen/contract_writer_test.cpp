#include "atlas/contract_gen/contract_writer.hpp"
#include "atlas/contract_gen/manifest.hpp"

#include <gtest/gtest.h>

namespace atlas::contract_gen {
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

TEST(GenerateContract, ReproducesTheHealthWorkedExample) {
    const Manifest manifest = parse_manifest(health_manifest_yaml);

    const std::string output = generate_contract(manifest, "health.capability.hpp", "health.capability.yaml");

    EXPECT_NE(output.find("// GENERATED — health.capability.hpp"), std::string::npos);
    EXPECT_NE(output.find("Source: health.capability.yaml — do not hand-edit."), std::string::npos);
    EXPECT_NE(output.find("namespace atlas::health {"), std::string::npos);

    EXPECT_NE(output.find("struct Health {"), std::string::npos);
    EXPECT_NE(output.find("std::int32_t current;"), std::string::npos);
    EXPECT_NE(output.find("std::int32_t maximum;"), std::string::npos);
    EXPECT_NE(output.find("static_assert(atlas::PropertyContract<Health>);"), std::string::npos);

    EXPECT_NE(output.find("struct ApplyDamage {"), std::string::npos);
    EXPECT_NE(output.find("atlas::EntityRef target;"), std::string::npos);
    EXPECT_NE(output.find("std::int32_t amount;"), std::string::npos);
    EXPECT_NE(output.find("static_assert(atlas::RequestContract<ApplyDamage>);"), std::string::npos);

    EXPECT_NE(output.find("struct HealthChanged {"), std::string::npos);
    EXPECT_NE(output.find("std::int32_t new_current;"), std::string::npos);
    EXPECT_NE(output.find("static_assert(atlas::EventContract<HealthChanged>);"), std::string::npos);

    // EntityRef is used (ApplyDamage.target, HealthChanged.target) - the
    // include must be present.
    EXPECT_NE(output.find("atlas/entity/entity_ref.hpp"), std::string::npos);

    // No DependsOn assertion: that needs a project-wide capability graph
    // (§5 cycle detection) this single-manifest generator round doesn't
    // have - deliberately omitted rather than emitted as a hollow always-true
    // placeholder.
    EXPECT_EQ(output.find("DependsOn"), std::string::npos);

    // Struct order must follow manifest declaration order.
    const auto health_pos = output.find("struct Health");
    const auto apply_damage_pos = output.find("struct ApplyDamage");
    const auto health_changed_pos = output.find("struct HealthChanged");
    ASSERT_NE(health_pos, std::string::npos);
    ASSERT_NE(apply_damage_pos, std::string::npos);
    ASSERT_NE(health_changed_pos, std::string::npos);
    EXPECT_LT(health_pos, apply_damage_pos);
    EXPECT_LT(apply_damage_pos, health_changed_pos);
}

TEST(GenerateContract, OmitsEntityRefIncludeWhenNoFieldUsesIt) {
    constexpr std::string_view yaml = R"(
capability:
  name: pure_numbers
properties:
  Score:
    value: int32
)";
    const Manifest manifest = parse_manifest(yaml);

    const std::string result =
        generate_contract(manifest, "pure_numbers.capability.hpp", "pure_numbers.capability.yaml");

    EXPECT_EQ(result.find("entity_ref.hpp"), std::string::npos);
}

TEST(GenerateContract, EmptyManifestProducesAnEmptyButValidNamespaceBlock) {
    constexpr std::string_view yaml = R"(
capability:
  name: nothing_yet
)";
    const Manifest manifest = parse_manifest(yaml);

    const std::string result =
        generate_contract(manifest, "nothing_yet.capability.hpp", "nothing_yet.capability.yaml");

    EXPECT_NE(result.find("namespace atlas::nothing_yet {"), std::string::npos);
    EXPECT_EQ(result.find("struct"), std::string::npos);
}

TEST(GenerateContract, RejectsAFieldTypeMapFieldTypeDoesNotRecognize) {
    // parse_manifest already validates every declared field type, so this
    // path is unreachable through the parser - construct a Manifest by hand
    // to prove generate_contract's own defensive check (it doesn't just
    // trust an already-validated caller) actually works.
    Manifest manifest;
    manifest.capability_name = "bad";
    manifest.properties.push_back(StructDecl{
        .name = "Bad",
        .fields = {Field{.name = "thing", .type = "not_a_real_type"}},
    });

    EXPECT_THROW((void)generate_contract(manifest, "bad.capability.hpp", "bad.capability.yaml"),
                 std::invalid_argument);
}

} // namespace
} // namespace atlas::contract_gen
