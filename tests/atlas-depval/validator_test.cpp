#include "atlas/cgen/dependency_graph.hpp"
#include "atlas/cgen/property_graph.hpp"
#include "atlas/depval/validator.hpp"

#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace atlas::depval {
namespace {

constexpr std::string_view movement_capability = R"(
capability:
  name: movement
)";

constexpr std::string_view haste_capability = R"(
capability:
  name: haste
depends_on: [movement]
)";

constexpr std::string_view gameplay_client_host = R"(
host: GameplayClient
composes:
  - movement
  - haste
)";

TEST(ValidateComposition, ResolvesASimpleTwoCapabilityHostInDependencyOrder) {
    const std::vector<std::string_view> capabilities{movement_capability, haste_capability};

    const ValidationReport report = validate_composition(gameplay_client_host, capabilities);

    EXPECT_EQ(report.host_name, "GameplayClient");
    ASSERT_EQ(report.composition_order.size(), 2U);
    EXPECT_EQ(report.composition_order[0], "movement");
    EXPECT_EQ(report.composition_order[1], "haste");
}

TEST(ValidateComposition, AnEmptyComposesListProducesAnEmptyOrderNamingTheHost) {
    constexpr std::string_view empty_host = R"(
host: EmptyHost
composes: []
)";
    const std::vector<std::string_view> capabilities{movement_capability};

    const ValidationReport report = validate_composition(empty_host, capabilities);

    EXPECT_EQ(report.host_name, "EmptyHost");
    EXPECT_TRUE(report.composition_order.empty());
}

TEST(ValidateComposition, ResolvesAProviderBeforeAConsumerDerivedPurelyFromConsumes) {
    // Proves the full merged depends_on + consumes-derived edge set is
    // actually exercised through this entry point, not just plain
    // depends_on (spec §5, Ordering Without Stages) - cast_time_attack names
    // no explicit depends_on on haste at all here.
    constexpr std::string_view haste_provider = R"(
capability:
  name: haste
properties:
  CastSpeed:
    value: float
)";
    constexpr std::string_view cast_time_attack_consumer = R"(
capability:
  name: cast_time_attack
consumes: [CastSpeed]
)";
    constexpr std::string_view host = R"(
host: GameplayClient
composes:
  - cast_time_attack
  - haste
)";
    const std::vector<std::string_view> capabilities{cast_time_attack_consumer, haste_provider};

    const ValidationReport report = validate_composition(host, capabilities);

    ASSERT_EQ(report.composition_order.size(), 2U);
    EXPECT_EQ(report.composition_order[0], "haste");
    EXPECT_EQ(report.composition_order[1], "cast_time_attack");
}

TEST(ValidateComposition, ThrowsWhenComposingAnUnknownCapability) {
    constexpr std::string_view host = R"(
host: GameplayClient
composes:
  - does_not_exist
)";
    const std::vector<std::string_view> capabilities{movement_capability};

    EXPECT_THROW({ (void)validate_composition(host, capabilities); }, std::invalid_argument);
}

TEST(ValidateComposition, ThrowsWhenAConsumedPropertyHasNoProviderAmongComposedCapabilities) {
    constexpr std::string_view cast_time_attack_consumer = R"(
capability:
  name: cast_time_attack
consumes: [CastSpeed]
)";
    constexpr std::string_view host = R"(
host: GameplayClient
composes:
  - cast_time_attack
)";
    const std::vector<std::string_view> capabilities{cast_time_attack_consumer};

    EXPECT_THROW(
        { (void)validate_composition(host, capabilities); }, atlas::cgen::UnresolvedPropertyConsumerError);
}

TEST(ValidateComposition, ThrowsADependencyCycleErrorNamingTheFullChainForAMultiEdgeCycle) {
    // spec §5: tooling must report the full chain of edges forming the
    // cycle, not just the first offending dependency - a three-capability
    // cycle (a -> b -> c -> a) is the minimum shape that actually
    // distinguishes "full chain" from "first edge found".
    constexpr std::string_view capability_a = R"(
capability:
  name: a
depends_on: [b]
)";
    constexpr std::string_view capability_b = R"(
capability:
  name: b
depends_on: [c]
)";
    constexpr std::string_view capability_c = R"(
capability:
  name: c
depends_on: [a]
)";
    constexpr std::string_view host = R"(
host: CyclicHost
composes:
  - a
  - b
  - c
)";
    const std::vector<std::string_view> capabilities{capability_a, capability_b, capability_c};

    try {
        (void)validate_composition(host, capabilities);
        FAIL() << "expected atlas::cgen::DependencyCycleError";
    } catch (const atlas::cgen::DependencyCycleError& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("a -> b -> c -> a"), std::string::npos) << message;
    }
}

TEST(FormatReport, RendersTheHostNameAndOneLinePerOrderedCapability) {
    const ValidationReport report{.host_name = "GameplayClient", .composition_order = {"movement", "haste"}};

    const std::string rendered = format_report(report);

    EXPECT_NE(rendered.find("GameplayClient"), std::string::npos);
    EXPECT_NE(rendered.find("movement"), std::string::npos);
    EXPECT_NE(rendered.find("haste"), std::string::npos);
    // Order matters in the rendered text too, not just the report struct.
    EXPECT_LT(rendered.find("movement"), rendered.find("haste"));
}

TEST(FormatReport, RendersAHostWithNoComposedCapabilitiesWithoutError) {
    const ValidationReport report{.host_name = "EmptyHost", .composition_order = {}};

    const std::string rendered = format_report(report);

    EXPECT_NE(rendered.find("EmptyHost"), std::string::npos);
}

} // namespace
} // namespace atlas::depval
