#include "atlas/cgen/manifest.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace atlas::cgen {
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

TEST(MapFieldType, MapsResourceId) {
    EXPECT_EQ(map_field_type("ResourceId"), "atlas::ResourceId");
}

TEST(MapFieldType, MapsSessionId) {
    EXPECT_EQ(map_field_type("SessionId"), "atlas::SessionId");
}

TEST(MapFieldType, MapsIntentId) {
    EXPECT_EQ(map_field_type("IntentId"), "atlas::input::IntentId");
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

TEST(RequiredIncludeForType, VocabularyTypesEachNameTheirOwnHeader) {
    EXPECT_EQ(required_include_for_type("EntityRef"), "atlas/entity/entity_ref.hpp");
    EXPECT_EQ(required_include_for_type("ResourceId"), "atlas/resource/resource_id.hpp");
    EXPECT_EQ(required_include_for_type("SessionId"), "atlas/session/session_id.hpp");
    EXPECT_EQ(required_include_for_type("IntentId"), "atlas/input/intent.hpp");
}

TEST(RequiredIncludeForType, PrimitiveTypesNeedNoDedicatedInclude) {
    // <cstdint>/bool/float/double are covered by the contract file template's
    // own unconditional includes - only vocabulary types need one per field.
    EXPECT_FALSE(required_include_for_type("int32").has_value());
    EXPECT_FALSE(required_include_for_type("bool").has_value());
    EXPECT_FALSE(required_include_for_type("float").has_value());
}

TEST(RequiredIncludeForType, UnknownTypeNeedsNoInclude) {
    EXPECT_FALSE(required_include_for_type("not_a_real_type").has_value());
}

TEST(MapCompositionStrategy, MapsEveryStrategyFromTheSpecTable) {
    EXPECT_EQ(map_composition_strategy("Additive"), "atlas::Composition::Additive");
    EXPECT_EQ(map_composition_strategy("Multiplicative"), "atlas::Composition::Multiplicative");
    EXPECT_EQ(map_composition_strategy("Override"), "atlas::Composition::Override");
    EXPECT_EQ(map_composition_strategy("PriorityOverride"), "atlas::Composition::PriorityOverride");
    EXPECT_EQ(map_composition_strategy("SetUnion"), "atlas::Composition::SetUnion");
    EXPECT_EQ(map_composition_strategy("OrderedComposition"), "atlas::Composition::OrderedComposition");
    EXPECT_EQ(map_composition_strategy("WeightedComposition"), "atlas::Composition::WeightedComposition");
}

TEST(MapCompositionStrategy, RejectsUnknownStrategyTokens) {
    EXPECT_FALSE(map_composition_strategy("not_a_real_strategy").has_value());
    EXPECT_FALSE(map_composition_strategy("").has_value());
    EXPECT_FALSE(
        map_composition_strategy("additive").has_value()); // case-sensitive, matches enumerator spelling
}

TEST(ParseManifest, PropertyWithACompositionKeyRecordsTheRawStrategyToken) {
    constexpr std::string_view text = R"(
capability:
  name: armor
properties:
  Armor:
    composition: Additive
    base: int32
)";

    const Manifest manifest = parse_manifest(text);

    ASSERT_EQ(manifest.properties.size(), 1U);
    ASSERT_TRUE(manifest.properties[0].composition.has_value());
    EXPECT_EQ(*manifest.properties[0].composition, "Additive");

    // 'composition' is consumed as metadata, not treated as an ordinary
    // field - only 'base' should appear in the field list.
    ASSERT_EQ(manifest.properties[0].fields.size(), 1U);
    EXPECT_EQ(manifest.properties[0].fields[0].name, "base");
    EXPECT_EQ(manifest.properties[0].fields[0].type, "int32");
}

TEST(ParseManifest, PropertyWithoutACompositionKeyLeavesItUnset) {
    const Manifest manifest = parse_manifest(health_manifest);

    ASSERT_EQ(manifest.properties.size(), 1U);
    EXPECT_FALSE(manifest.properties[0].composition.has_value());
}

TEST(ParseManifest, RejectsUnrecognizedCompositionStrategy) {
    constexpr std::string_view text = R"(
capability:
  name: bad
properties:
  Bad:
    composition: not_a_real_strategy
    base: int32
)";

    try {
        (void)parse_manifest(text);
        FAIL() << "expected parse_manifest to throw";
    } catch (const std::invalid_argument& e) {
        EXPECT_NE(std::string_view(e.what()).find("not_a_real_strategy"), std::string_view::npos);
    }
}

TEST(ParseManifest, PropertyWithATriggerKeyRecordsIt) {
    constexpr std::string_view text = R"(
capability:
  name: movement
properties:
  PositionChanged:
    trigger: true
    new_x: float
    new_y: float
)";

    const Manifest manifest = parse_manifest(text);

    ASSERT_EQ(manifest.properties.size(), 1U);
    EXPECT_TRUE(manifest.properties[0].trigger);

    // 'trigger' is consumed as metadata, not treated as an ordinary field -
    // only 'new_x'/'new_y' should appear in the field list.
    ASSERT_EQ(manifest.properties[0].fields.size(), 2U);
    EXPECT_EQ(manifest.properties[0].fields[0].name, "new_x");
    EXPECT_EQ(manifest.properties[0].fields[1].name, "new_y");
}

TEST(ParseManifest, PropertyWithoutATriggerKeyDefaultsToFalse) {
    const Manifest manifest = parse_manifest(health_manifest);

    ASSERT_EQ(manifest.properties.size(), 1U);
    EXPECT_FALSE(manifest.properties[0].trigger);
}

TEST(ParseManifest, TriggerKeyOnARequestIsTreatedAsAnOrdinaryFieldAndRejected) {
    // Trigger is a property-only concept (spec §20, Triggered composition) -
    // requests/events don't get special-cased handling for this key, so a
    // manifest author who mistakenly writes it there gets an honest
    // "unrecognized type" error (since "true" isn't a valid field type)
    // rather than silent acceptance.
    constexpr std::string_view text = R"(
capability:
  name: bad
requests:
  BadRequest:
    trigger: true
)";

    EXPECT_THROW((void)parse_manifest(text), std::invalid_argument);
}

TEST(ParseManifest, ParsesConsumesAlongsideDependsOn) {
    constexpr std::string_view text = R"(
capability:
  name: cast_time_attack
depends_on: [entity]
consumes: [CastSpeed]
)";

    const Manifest manifest = parse_manifest(text);

    ASSERT_EQ(manifest.consumes.size(), 1U);
    EXPECT_EQ(manifest.consumes[0], "CastSpeed");
}

TEST(ParseManifest, ConsumesDefaultsToEmptyWhenAbsent) {
    const Manifest manifest = parse_manifest(health_manifest);

    EXPECT_TRUE(manifest.consumes.empty());
}

TEST(ParseManifest, CompositionKeyOnARequestIsTreatedAsAnOrdinaryFieldAndRejected) {
    // Composition is a property-only concept (spec §20) - requests/events
    // don't get special-cased handling for this key, so a manifest author
    // who mistakenly writes it there gets an honest "unrecognized type"
    // error (since "Additive" isn't a valid field type) rather than silent
    // acceptance.
    constexpr std::string_view text = R"(
capability:
  name: bad
requests:
  BadRequest:
    composition: Additive
)";

    EXPECT_THROW((void)parse_manifest(text), std::invalid_argument);
}

} // namespace
} // namespace atlas::cgen
