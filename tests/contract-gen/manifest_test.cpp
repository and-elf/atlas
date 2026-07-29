#include "atlas/contract_gen/manifest.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace atlas::contract_gen {
namespace {

constexpr std::string_view health_manifest = R"(
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

TEST(ParseManifest, ParsesTheHealthWorkedExample) {
    const Manifest manifest = parse_manifest(health_manifest);

    EXPECT_EQ(manifest.capability_name, "health");
    ASSERT_EQ(manifest.depends_on.size(), 1U);
    EXPECT_EQ(manifest.depends_on[0], "entity");

    ASSERT_EQ(manifest.properties.size(), 1U);
    EXPECT_EQ(manifest.properties[0].name, "Health");
    ASSERT_EQ(manifest.properties[0].fields.size(), 2U);
    EXPECT_EQ(manifest.properties[0].fields[0].name, "current");
    EXPECT_EQ(manifest.properties[0].fields[0].type, "int32");
    EXPECT_EQ(manifest.properties[0].fields[1].name, "maximum");
    EXPECT_EQ(manifest.properties[0].fields[1].type, "int32");

    ASSERT_EQ(manifest.requests.size(), 1U);
    EXPECT_EQ(manifest.requests[0].name, "ApplyDamage");
    ASSERT_EQ(manifest.requests[0].fields.size(), 2U);
    EXPECT_EQ(manifest.requests[0].fields[0].name, "target");
    EXPECT_EQ(manifest.requests[0].fields[0].type, "EntityRef");
    EXPECT_EQ(manifest.requests[0].fields[1].name, "amount");
    EXPECT_EQ(manifest.requests[0].fields[1].type, "int32");

    ASSERT_EQ(manifest.events.size(), 1U);
    EXPECT_EQ(manifest.events[0].name, "HealthChanged");
    ASSERT_EQ(manifest.events[0].fields.size(), 2U);
    EXPECT_EQ(manifest.events[0].fields[0].name, "target");
    EXPECT_EQ(manifest.events[0].fields[0].type, "EntityRef");
    EXPECT_EQ(manifest.events[0].fields[1].name, "new_current");
    EXPECT_EQ(manifest.events[0].fields[1].type, "int32");
}

TEST(ParseManifest, DependsOnDefaultsToEmptyWhenAbsent) {
    constexpr std::string_view text = R"(
capability:
  name: minimal
)";

    const Manifest manifest = parse_manifest(text);

    EXPECT_TRUE(manifest.depends_on.empty());
    EXPECT_TRUE(manifest.properties.empty());
    EXPECT_TRUE(manifest.requests.empty());
    EXPECT_TRUE(manifest.events.empty());
}

TEST(ParseManifest, RejectsMissingCapabilityBlock) {
    constexpr std::string_view text = R"(
properties:
  Health:
    current: int32
)";

    EXPECT_THROW((void)parse_manifest(text), std::invalid_argument);
}

TEST(ParseManifest, RejectsMissingCapabilityName) {
    constexpr std::string_view text = R"(
capability:
  version: 1.0.0
)";

    EXPECT_THROW((void)parse_manifest(text), std::invalid_argument);
}

TEST(ParseManifest, RejectsMalformedYaml) {
    constexpr std::string_view text = "capability: [this is not a mapping";

    EXPECT_THROW((void)parse_manifest(text), std::invalid_argument);
}

TEST(ParseManifest, RejectsNonMappingDocumentRoot) {
    constexpr std::string_view text = "- just\n- a\n- sequence\n";

    EXPECT_THROW((void)parse_manifest(text), std::invalid_argument);
}

TEST(ParseManifest, RejectsNonMappingStructBlock) {
    constexpr std::string_view text = R"(
capability:
  name: bad
properties: [not, a, mapping]
)";

    EXPECT_THROW((void)parse_manifest(text), std::invalid_argument);
}

TEST(ParseManifest, RejectsStructEntryThatIsNotAMapping) {
    constexpr std::string_view text = R"(
capability:
  name: bad
properties:
  Health: not_a_mapping_of_fields
)";

    EXPECT_THROW((void)parse_manifest(text), std::invalid_argument);
}

TEST(ParseManifest, RejectsUnrecognizedFieldType) {
    constexpr std::string_view text = R"(
capability:
  name: bad
properties:
  Bad:
    thing: not_a_real_type
)";

    try {
        (void)parse_manifest(text);
        FAIL() << "expected parse_manifest to throw";
    } catch (const std::invalid_argument& e) {
        EXPECT_NE(std::string_view(e.what()).find("not_a_real_type"), std::string_view::npos);
    }
}

TEST(ParseManifest, IgnoresUnknownTopLevelKeysForForwardCompatibility) {
    // Real manifests also carry `version`, `source`, `contracts` (spec §13) -
    // this generator round only supports capability/depends_on/properties/
    // requests/events, so unknown keys must be ignored, not rejected.
    constexpr std::string_view text = R"(
capability:
  name: health
  version: 1.4.0
source:
  root: src/
  files: [health.cpp]
)";

    const Manifest manifest = parse_manifest(text);

    EXPECT_EQ(manifest.capability_name, "health");
}

TEST(MapFieldType, MapsEveryTypeTheHealthExampleUses) {
    EXPECT_EQ(map_field_type("int32"), "std::int32_t");
    EXPECT_EQ(map_field_type("EntityRef"), "atlas::EntityRef");
}

TEST(MapFieldType, MapsTheRestOfTheClosedTypeSet) {
    EXPECT_EQ(map_field_type("int8"), "std::int8_t");
    EXPECT_EQ(map_field_type("int16"), "std::int16_t");
    EXPECT_EQ(map_field_type("int64"), "std::int64_t");
    EXPECT_EQ(map_field_type("uint8"), "std::uint8_t");
    EXPECT_EQ(map_field_type("uint16"), "std::uint16_t");
    EXPECT_EQ(map_field_type("uint32"), "std::uint32_t");
    EXPECT_EQ(map_field_type("uint64"), "std::uint64_t");
    EXPECT_EQ(map_field_type("float"), "float");
    EXPECT_EQ(map_field_type("double"), "double");
    EXPECT_EQ(map_field_type("bool"), "bool");
}

TEST(MapFieldType, RejectsUnknownTypeTokens) {
    EXPECT_FALSE(map_field_type("not_a_real_type").has_value());
    EXPECT_FALSE(map_field_type("").has_value());
}

} // namespace
} // namespace atlas::contract_gen
