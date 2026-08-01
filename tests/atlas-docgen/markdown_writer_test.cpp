#include "atlas/cgen/manifest.hpp"
#include "atlas/docgen/markdown_writer.hpp"

#include <gtest/gtest.h>

namespace atlas::docgen {
namespace {

using atlas::cgen::Field;
using atlas::cgen::Manifest;
using atlas::cgen::StructDecl;

// Identical to tests/fixtures/health.capability.yaml - the same fixture
// atlas-cgen's own contract_writer_test.cpp reproduces inline, reused here
// so this generator round is proven against real §21 worked-example
// content rather than an invented shape.
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

TEST(GenerateMarkdownDoc, ReproducesTheHealthWorkedExample) {
    const Manifest manifest = atlas::cgen::parse_manifest(health_manifest_yaml);

    const std::string result = generate_markdown_doc(manifest, "health.md", "health.capability.yaml");

    EXPECT_NE(result.find("GENERATED"), std::string::npos);
    EXPECT_NE(result.find("health.md"), std::string::npos);
    EXPECT_NE(result.find("Source: health.capability.yaml"), std::string::npos);
    EXPECT_NE(result.find("do not hand-edit"), std::string::npos);

    EXPECT_NE(result.find("# health"), std::string::npos);
    EXPECT_NE(result.find("**Depends on:** entity"), std::string::npos);

    EXPECT_NE(result.find("## Properties"), std::string::npos);
    EXPECT_NE(result.find("### Health"), std::string::npos);
    EXPECT_NE(result.find("| current | int32 |"), std::string::npos);
    EXPECT_NE(result.find("| maximum | int32 |"), std::string::npos);

    EXPECT_NE(result.find("## Requests"), std::string::npos);
    EXPECT_NE(result.find("### ApplyDamage"), std::string::npos);
    EXPECT_NE(result.find("| target | EntityRef |"), std::string::npos);
    EXPECT_NE(result.find("| amount | int32 |"), std::string::npos);

    EXPECT_NE(result.find("## Events"), std::string::npos);
    EXPECT_NE(result.find("### HealthChanged"), std::string::npos);
    EXPECT_NE(result.find("| new_current | int32 |"), std::string::npos);

    // Struct order must follow manifest declaration order, and Properties
    // must precede Requests must precede Events - matching
    // atlas::cgen::generate_contract's own fixed block order.
    const auto properties_pos = result.find("## Properties");
    const auto requests_pos = result.find("## Requests");
    const auto events_pos = result.find("## Events");
    ASSERT_NE(properties_pos, std::string::npos);
    ASSERT_NE(requests_pos, std::string::npos);
    ASSERT_NE(events_pos, std::string::npos);
    EXPECT_LT(properties_pos, requests_pos);
    EXPECT_LT(requests_pos, events_pos);
}

TEST(GenerateMarkdownDoc, ShowsNoneWhenDependsOnIsEmpty) {
    constexpr std::string_view yaml = R"(
capability:
  name: nothing_yet
)";
    const Manifest manifest = atlas::cgen::parse_manifest(yaml);

    const std::string result =
        generate_markdown_doc(manifest, "nothing_yet.md", "nothing_yet.capability.yaml");

    EXPECT_NE(result.find("**Depends on:** _(none)_"), std::string::npos);
}

TEST(GenerateMarkdownDoc, ShowsNoneWhenConsumesIsEmpty) {
    constexpr std::string_view yaml = R"(
capability:
  name: nothing_yet
)";
    const Manifest manifest = atlas::cgen::parse_manifest(yaml);

    const std::string result =
        generate_markdown_doc(manifest, "nothing_yet.md", "nothing_yet.capability.yaml");

    EXPECT_NE(result.find("**Consumes:** _(none)_"), std::string::npos);
}

TEST(GenerateMarkdownDoc, ListsConsumesEntriesWhenPresent) {
    constexpr std::string_view yaml = R"(
capability:
  name: cast_time_attack
consumes: [CastSpeed]
)";
    const Manifest manifest = atlas::cgen::parse_manifest(yaml);

    const std::string result =
        generate_markdown_doc(manifest, "cast_time_attack.md", "cast_time_attack.capability.yaml");

    EXPECT_NE(result.find("**Consumes:** CastSpeed"), std::string::npos);
    // depends_on is empty in this fixture (a capability may legitimately
    // consume properties without any explicit depends_on entry) - only the
    // Consumes line's own "_(none)_" is what this test cares about.
    EXPECT_EQ(result.find("**Consumes:** _(none)_"), std::string::npos);
}

TEST(GenerateMarkdownDoc, EmptyManifestOmitsAllSectionHeadings) {
    constexpr std::string_view yaml = R"(
capability:
  name: nothing_yet
)";
    const Manifest manifest = atlas::cgen::parse_manifest(yaml);

    const std::string result =
        generate_markdown_doc(manifest, "nothing_yet.md", "nothing_yet.capability.yaml");

    EXPECT_EQ(result.find("## Properties"), std::string::npos);
    EXPECT_EQ(result.find("## Requests"), std::string::npos);
    EXPECT_EQ(result.find("## Events"), std::string::npos);
}

TEST(GenerateMarkdownDoc, ComposedPropertyIsAnnotatedWithItsStrategy) {
    constexpr std::string_view yaml = R"(
capability:
  name: armor
properties:
  Armor:
    composition: Additive
    base: int32
)";
    const Manifest manifest = atlas::cgen::parse_manifest(yaml);

    const std::string result = generate_markdown_doc(manifest, "armor.md", "armor.capability.yaml");

    EXPECT_NE(result.find("_Composition strategy: Additive_"), std::string::npos);

    // The annotation must appear before the field table, matching
    // generate_contract's own "composition member before ordinary fields"
    // convention (spec §20's generated-contract example).
    const auto annotation_pos = result.find("_Composition strategy: Additive_");
    const auto field_pos = result.find("| base | int32 |");
    ASSERT_NE(annotation_pos, std::string::npos);
    ASSERT_NE(field_pos, std::string::npos);
    EXPECT_LT(annotation_pos, field_pos);
}

TEST(GenerateMarkdownDoc, TriggeredPropertyIsAnnotatedAsTriggered) {
    constexpr std::string_view yaml = R"(
capability:
  name: movement
properties:
  PositionChanged:
    trigger: true
    new_x: float
    new_y: float
)";
    const Manifest manifest = atlas::cgen::parse_manifest(yaml);

    const std::string result = generate_markdown_doc(manifest, "movement.md", "movement.capability.yaml");

    EXPECT_NE(result.find("_Triggered:"), std::string::npos);

    const auto annotation_pos = result.find("_Triggered:");
    const auto field_pos = result.find("| new_x | float |");
    ASSERT_NE(annotation_pos, std::string::npos);
    ASSERT_NE(field_pos, std::string::npos);
    EXPECT_LT(annotation_pos, field_pos);
}

TEST(GenerateMarkdownDoc, NonComposedNonTriggeredPropertyHasNoAnnotationLine) {
    const Manifest manifest = atlas::cgen::parse_manifest(health_manifest_yaml);

    const std::string result = generate_markdown_doc(manifest, "health.md", "health.capability.yaml");

    EXPECT_EQ(result.find("_Composition strategy:"), std::string::npos);
    EXPECT_EQ(result.find("_Triggered:"), std::string::npos);
}

TEST(GenerateMarkdownDoc, BothComposedAndTriggeredAnnotationsCanAppearTogether) {
    constexpr std::string_view yaml = R"(
capability:
  name: aura
properties:
  AuraPulse:
    composition: Additive
    trigger: true
    strength: float
)";
    const Manifest manifest = atlas::cgen::parse_manifest(yaml);

    const std::string result = generate_markdown_doc(manifest, "aura.md", "aura.capability.yaml");

    EXPECT_NE(result.find("_Composition strategy: Additive_"), std::string::npos);
    EXPECT_NE(result.find("_Triggered:"), std::string::npos);
}

} // namespace
} // namespace atlas::docgen
