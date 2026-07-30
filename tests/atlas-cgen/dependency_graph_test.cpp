#include "atlas/cgen/dependency_graph.hpp"

#include <algorithm>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace atlas::cgen {
namespace {

TEST(DependencyGraph, OrdersASingleNodeWithNoDependencies) {
    const std::vector<DependencyNode> nodes{{.name = "entity", .depends_on = {}}};

    const CompositionOrder order = resolve_composition_order(nodes);

    ASSERT_EQ(order.size(), 1U);
    EXPECT_EQ(order[0], "entity");
}

TEST(DependencyGraph, OrdersADependencyBeforeItsDependent) {
    const std::vector<DependencyNode> nodes{
        {.name = "movement", .depends_on = {"entity"}},
        {.name = "entity", .depends_on = {}},
    };

    const CompositionOrder order = resolve_composition_order(nodes);

    ASSERT_EQ(order.size(), 2U);
    const auto entity_pos = std::find(order.begin(), order.end(), "entity");
    const auto movement_pos = std::find(order.begin(), order.end(), "movement");
    EXPECT_LT(entity_pos, movement_pos);
}

TEST(DependencyGraph, OrdersATransitiveChainCorrectly) {
    // haste -> movement -> entity: entity first, haste last, regardless of
    // input order.
    const std::vector<DependencyNode> nodes{
        {.name = "haste", .depends_on = {"movement"}},
        {.name = "movement", .depends_on = {"entity"}},
        {.name = "entity", .depends_on = {}},
    };

    const CompositionOrder order = resolve_composition_order(nodes);

    ASSERT_EQ(order.size(), 3U);
    EXPECT_EQ(order[0], "entity");
    EXPECT_EQ(order[1], "movement");
    EXPECT_EQ(order[2], "haste");
}

TEST(DependencyGraph, ADiamondDependencyOrdersTheSharedBaseOnlyOnce) {
    // cast_time_attack -> {movement, haste}, haste -> movement, movement ->
    // entity. entity/movement must each appear exactly once, before
    // everything that depends on them.
    const std::vector<DependencyNode> nodes{
        {.name = "cast_time_attack", .depends_on = {"movement", "haste"}},
        {.name = "haste", .depends_on = {"movement"}},
        {.name = "movement", .depends_on = {"entity"}},
        {.name = "entity", .depends_on = {}},
    };

    const CompositionOrder order = resolve_composition_order(nodes);

    ASSERT_EQ(order.size(), 4U);
    EXPECT_EQ(std::count(order.begin(), order.end(), "movement"), 1);
    EXPECT_EQ(std::count(order.begin(), order.end(), "entity"), 1);
    const auto entity_pos = std::find(order.begin(), order.end(), "entity");
    const auto movement_pos = std::find(order.begin(), order.end(), "movement");
    const auto haste_pos = std::find(order.begin(), order.end(), "haste");
    const auto cast_pos = std::find(order.begin(), order.end(), "cast_time_attack");
    EXPECT_LT(entity_pos, movement_pos);
    EXPECT_LT(movement_pos, haste_pos);
    EXPECT_LT(haste_pos, cast_pos);
}

TEST(DependencyGraph, ThrowsOnAnUnknownDependency) {
    const std::vector<DependencyNode> nodes{
        {.name = "movement", .depends_on = {"entity"}},
        // "entity" is never declared as its own node.
    };

    EXPECT_THROW({ (void)resolve_composition_order(nodes); }, std::invalid_argument);
}

TEST(DependencyGraph, ThrowsOnADuplicateNodeName) {
    const std::vector<DependencyNode> nodes{
        {.name = "movement", .depends_on = {}},
        {.name = "movement", .depends_on = {}},
    };

    EXPECT_THROW({ (void)resolve_composition_order(nodes); }, std::invalid_argument);
}

TEST(DependencyGraph, ThrowsADependencyCycleErrorOnADirectCycle) {
    const std::vector<DependencyNode> nodes{
        {.name = "a", .depends_on = {"b"}},
        {.name = "b", .depends_on = {"a"}},
    };

    EXPECT_THROW({ (void)resolve_composition_order(nodes); }, DependencyCycleError);
}

TEST(DependencyGraph, DependencyCycleErrorNamesTheFullChainInOrder) {
    // spec §5: tooling must report the full chain of edges forming the
    // cycle, not just the first offending dependency.
    const std::vector<DependencyNode> nodes{
        {.name = "a", .depends_on = {"b"}},
        {.name = "b", .depends_on = {"c"}},
        {.name = "c", .depends_on = {"a"}},
    };

    try {
        (void)resolve_composition_order(nodes);
        FAIL() << "expected DependencyCycleError";
    } catch (const DependencyCycleError& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find('a'), std::string::npos);
        EXPECT_NE(message.find('b'), std::string::npos);
        EXPECT_NE(message.find('c'), std::string::npos);
        // The chain must actually be in edge order (a -> b -> c -> a), not
        // just mention all three names in any order.
        EXPECT_NE(message.find("a -> b -> c -> a"), std::string::npos);
    }
}

TEST(DependencyGraph, ASelfDependencyIsACycle) {
    const std::vector<DependencyNode> nodes{{.name = "a", .depends_on = {"a"}}};

    EXPECT_THROW({ (void)resolve_composition_order(nodes); }, DependencyCycleError);
}

TEST(DependencyGraph, EmptyInputProducesAnEmptyOrder) {
    const std::vector<DependencyNode> nodes;

    EXPECT_TRUE(resolve_composition_order(nodes).empty());
}

} // namespace
} // namespace atlas::cgen
