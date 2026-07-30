#include "atlas/cgen/dependency_graph.hpp"
#include "atlas/cgen/host_composition.hpp"

#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

namespace atlas::cgen {
namespace {

Manifest make_manifest(std::string name, std::vector<std::string> depends_on) {
    return Manifest{
        .capability_name = std::move(name),
        .depends_on = std::move(depends_on),
        .properties = {},
        .requests = {},
        .events = {},
    };
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

} // namespace
} // namespace atlas::cgen
