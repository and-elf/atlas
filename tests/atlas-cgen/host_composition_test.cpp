#include "atlas/cgen/dependency_graph.hpp"
#include "atlas/cgen/host_composition.hpp"
#include "atlas/cgen/property_graph.hpp"

#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

namespace atlas::cgen {
namespace {

Manifest make_manifest(std::string name, std::vector<std::string> depends_on) {
    return Manifest{
        .capability_name = std::move(name),
        .depends_on = std::move(depends_on),
        .consumes = {},
        .properties = {},
        .requests = {},
        .events = {},
    };
}

// consumes/provides bundled into one struct rather than two adjacent
// std::vector<std::string> parameters (bugprone-easily-swappable-parameters).
struct ManifestPropertySpec {
    std::vector<std::string> consumes;
    std::vector<std::string> provides;
};

// A manifest that both provides a property (so another manifest can consume
// it) and/or consumes properties of its own - the extra shape this issue's
// property-graph tests need, kept as a separate overload rather than
// widening make_manifest's signature for every existing call site above.
Manifest
make_property_manifest(std::string name, std::vector<std::string> depends_on, ManifestPropertySpec spec) {
    Manifest manifest{
        .capability_name = std::move(name),
        .depends_on = std::move(depends_on),
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

TEST(ResolveHostComposition, IgnoresAFoundationalDependencyNotAmongAvailableManifests) {
    // "entity" is a foundational runtime dependency (atlas-entity), never
    // its own capability manifest - depending on it must not require
    // composing it, and must not error just because it's absent from
    // available_manifests.
    const std::vector<Manifest> available{make_manifest("movement", {"entity"})};
    const HostManifest host{.host_name = "GameplayClient", .composes = {"movement"}};

    const HostComposition composition = resolve_host_composition(host, available);

    ASSERT_EQ(composition.ordered_capabilities.size(), 1U);
    EXPECT_EQ(composition.ordered_capabilities[0].capability_name, "movement");
    EXPECT_EQ(composition.host_name, "GameplayClient");
}

TEST(ResolveHostComposition, OrdersADependencyBeforeItsDependent) {
    const std::vector<Manifest> available{
        make_manifest("haste", {"movement"}),
        make_manifest("movement", {"entity"}),
    };
    const HostManifest host{.host_name = "GameplayClient", .composes = {"haste", "movement"}};

    const HostComposition composition = resolve_host_composition(host, available);

    ASSERT_EQ(composition.ordered_capabilities.size(), 2U);
    EXPECT_EQ(composition.ordered_capabilities[0].capability_name, "movement");
    EXPECT_EQ(composition.ordered_capabilities[1].capability_name, "haste");
}

TEST(ResolveHostComposition, ThrowsWhenComposingAnUnknownCapability) {
    const std::vector<Manifest> available{make_manifest("movement", {})};
    const HostManifest host{.host_name = "GameplayClient", .composes = {"does_not_exist"}};

    EXPECT_THROW({ (void)resolve_host_composition(host, available); }, std::invalid_argument);
}

TEST(ResolveHostComposition, ThrowsWhenAKnownDependencyIsNotComposed) {
    // haste depends on movement, and movement DOES have a manifest among
    // available - composing haste without movement must be a hard error,
    // not silent (implicit transitive inclusion isn't this generator's
    // model - composition is explicit).
    const std::vector<Manifest> available{
        make_manifest("haste", {"movement"}),
        make_manifest("movement", {"entity"}),
    };
    const HostManifest host{.host_name = "GameplayClient", .composes = {"haste"}};

    EXPECT_THROW({ (void)resolve_host_composition(host, available); }, std::invalid_argument);
}

TEST(ResolveHostComposition, ThrowsOnDuplicateCapabilityManifestNames) {
    const std::vector<Manifest> available{
        make_manifest("movement", {}),
        make_manifest("movement", {}),
    };
    const HostManifest host{.host_name = "GameplayClient", .composes = {"movement"}};

    EXPECT_THROW({ (void)resolve_host_composition(host, available); }, std::invalid_argument);
}

TEST(ResolveHostComposition, PropagatesACycleAsADependencyCycleError) {
    const std::vector<Manifest> available{
        make_manifest("a", {"b"}),
        make_manifest("b", {"a"}),
    };
    const HostManifest host{.host_name = "GameplayClient", .composes = {"a", "b"}};

    EXPECT_THROW({ (void)resolve_host_composition(host, available); }, DependencyCycleError);
}

TEST(ResolveHostComposition, AnEmptyComposesListProducesAnEmptyComposition) {
    const std::vector<Manifest> available{make_manifest("movement", {})};
    const HostManifest host{.host_name = "EmptyHost", .composes = {}};

    const HostComposition composition = resolve_host_composition(host, available);

    EXPECT_TRUE(composition.ordered_capabilities.empty());
}

TEST(ResolveHostComposition, OrdersAProviderBeforeAConsumerDerivedPurelyFromConsumes) {
    // cast_time_attack names no explicit depends_on on haste at all - the
    // edge is derived entirely from consumes: [CastSpeed] resolving against
    // haste's own properties: block (issue #16: "systems coupled to data,
    // not implementation").
    const std::vector<Manifest> available{
        make_property_manifest("cast_time_attack", {}, {.consumes = {"CastSpeed"}, .provides = {}}),
        make_property_manifest("haste", {}, {.consumes = {}, .provides = {"CastSpeed"}}),
    };
    const HostManifest host{.host_name = "GameplayClient", .composes = {"cast_time_attack", "haste"}};

    const HostComposition composition = resolve_host_composition(host, available);

    ASSERT_EQ(composition.ordered_capabilities.size(), 2U);
    EXPECT_EQ(composition.ordered_capabilities[0].capability_name, "haste");
    EXPECT_EQ(composition.ordered_capabilities[1].capability_name, "cast_time_attack");
}

TEST(ResolveHostComposition, ThrowsWhenAConsumedPropertyHasNoProviderAmongComposedCapabilities) {
    const std::vector<Manifest> available{
        make_property_manifest("cast_time_attack", {}, {.consumes = {"CastSpeed"}, .provides = {}}),
    };
    const HostManifest host{.host_name = "GameplayClient", .composes = {"cast_time_attack"}};

    EXPECT_THROW({ (void)resolve_host_composition(host, available); }, UnresolvedPropertyConsumerError);
}

TEST(ResolveHostComposition, ThrowsWhenTwoComposedCapabilitiesProvideTheSamePropertyName) {
    const std::vector<Manifest> available{
        make_property_manifest("core_combat", {}, {.consumes = {}, .provides = {"AttackPower"}}),
        make_property_manifest("rpg_expansion", {}, {.consumes = {}, .provides = {"AttackPower"}}),
    };
    const HostManifest host{.host_name = "GameplayClient", .composes = {"core_combat", "rpg_expansion"}};

    EXPECT_THROW({ (void)resolve_host_composition(host, available); }, PropertyProviderConflictError);
}

} // namespace
} // namespace atlas::cgen
