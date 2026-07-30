#include "atlas/cgen/property_graph.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace atlas::cgen {
namespace {

// consumes/provides bundled into one struct rather than two adjacent
// std::vector<std::string> parameters (bugprone-easily-swappable-parameters).
struct ManifestPropertySpec {
    std::vector<std::string> consumes;
    std::vector<std::string> provides;
};

Manifest make_manifest(std::string name, ManifestPropertySpec spec) {
    Manifest manifest{
        .capability_name = std::move(name),
        .depends_on = {},
        .consumes = std::move(spec.consumes),
        .properties = {},
        .requests = {},
        .events = {},
    };
    for (auto& property_name : spec.provides) {
        manifest.properties.push_back(
            StructDecl{.name = std::move(property_name), .fields = {}, .composition = std::nullopt});
    }
    return manifest;
}

TEST(ResolvePropertyProviders, MapsEachDeclaredPropertyToItsOwningCapability) {
    const std::vector<Manifest> manifests{
        make_manifest("movement", {.consumes = {}, .provides = {"Position", "MovementSpeed"}}),
        make_manifest("haste", {.consumes = {}, .provides = {"CastSpeed"}}),
    };

    const auto providers = resolve_property_providers(manifests);

    ASSERT_EQ(providers.size(), 3U);
    EXPECT_EQ(providers.at("Position"), "movement");
    EXPECT_EQ(providers.at("MovementSpeed"), "movement");
    EXPECT_EQ(providers.at("CastSpeed"), "haste");
}

TEST(ResolvePropertyProviders, EmptyManifestListProducesAnEmptyMap) {
    const std::vector<Manifest> manifests{};

    EXPECT_TRUE(resolve_property_providers(manifests).empty());
}

TEST(ResolvePropertyProviders, ThrowsWhenTwoManifestsDeclareTheSamePropertyName) {
    const std::vector<Manifest> manifests{
        make_manifest("core_combat", {.consumes = {}, .provides = {"AttackPower"}}),
        make_manifest("rpg_expansion", {.consumes = {}, .provides = {"AttackPower"}}),
    };

    try {
        (void)resolve_property_providers(manifests);
        FAIL() << "expected resolve_property_providers to throw";
    } catch (const PropertyProviderConflictError& e) {
        EXPECT_NE(std::string_view(e.what()).find("AttackPower"), std::string_view::npos);
        EXPECT_NE(std::string_view(e.what()).find("core_combat"), std::string_view::npos);
        EXPECT_NE(std::string_view(e.what()).find("rpg_expansion"), std::string_view::npos);
    }
}

TEST(ResolvePropertyProviders, IsUnaffectedByAManifestDeclaringNoProperties) {
    const std::vector<Manifest> manifests{
        make_manifest("interruption", {.consumes = {}, .provides = {}}),
        make_manifest("movement", {.consumes = {}, .provides = {"Position"}}),
    };

    const auto providers = resolve_property_providers(manifests);

    EXPECT_EQ(providers.size(), 1U);
    EXPECT_EQ(providers.at("Position"), "movement");
}

TEST(ResolvePropertyDependencyEdges, DerivesAnEdgeFromConsumerToProvider) {
    const std::vector<Manifest> composed{
        make_manifest("cast_time_attack", {.consumes = {"CastSpeed"}, .provides = {}}),
        make_manifest("haste", {.consumes = {}, .provides = {"CastSpeed"}}),
    };
    const auto providers = resolve_property_providers(composed);

    const auto edges = resolve_property_dependency_edges(composed, providers);

    ASSERT_EQ(edges.count("cast_time_attack"), 1U);
    ASSERT_EQ(edges.at("cast_time_attack").size(), 1U);
    EXPECT_EQ(edges.at("cast_time_attack")[0], "haste");
    EXPECT_EQ(edges.count("haste"), 0U);
}

TEST(ResolvePropertyDependencyEdges, AManifestWithNoConsumesEntriesProducesNoEdges) {
    const std::vector<Manifest> composed{
        make_manifest("movement", {.consumes = {}, .provides = {"Position"}})};
    const auto providers = resolve_property_providers(composed);

    const auto edges = resolve_property_dependency_edges(composed, providers);

    EXPECT_TRUE(edges.empty());
}

TEST(ResolvePropertyDependencyEdges, ConsumingAPropertyItProvidesItselfProducesNoEdge) {
    // A capability naming its own property in consumes: is not an ordering
    // error - there's nothing to order it before - so this must not
    // introduce a trivial self-dependency edge (which resolve_composition_order
    // would otherwise have to special-case as a self-cycle).
    const std::vector<Manifest> composed{
        make_manifest("haste", {.consumes = {"CastSpeed"}, .provides = {"CastSpeed"}})};
    const auto providers = resolve_property_providers(composed);

    const auto edges = resolve_property_dependency_edges(composed, providers);

    EXPECT_TRUE(edges.empty());
}

TEST(ResolvePropertyDependencyEdges, ThrowsWhenAConsumedPropertyHasNoProvider) {
    const std::vector<Manifest> composed{
        make_manifest("cast_time_attack", {.consumes = {"CastSpeed"}, .provides = {}})};
    const auto providers = resolve_property_providers(composed);

    try {
        (void)resolve_property_dependency_edges(composed, providers);
        FAIL() << "expected resolve_property_dependency_edges to throw";
    } catch (const UnresolvedPropertyConsumerError& e) {
        EXPECT_NE(std::string_view(e.what()).find("CastSpeed"), std::string_view::npos);
        EXPECT_NE(std::string_view(e.what()).find("cast_time_attack"), std::string_view::npos);
    }
}

} // namespace
} // namespace atlas::cgen
