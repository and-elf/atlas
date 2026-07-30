// Proves line_of_sight::blocks_line_of_sight - the standard point-to-segment
// distance construction, not a request (spec §6 authority validation
// doesn't apply to a pure query that mutates nothing). Every case is a
// concrete worked scenario (source at the origin, target 10 units along
// +x) rather than an assertion on an opaque boolean, the same rigor
// movement_test.cpp and aura_test.cpp already establish.
#include <gtest/gtest.h>
#include <stdexcept>

#include "line_of_sight/line_of_sight.hpp"
#include "simulated_host.hpp"

namespace atlas::demo {
namespace {

using testing::SimulatedHost;

TEST(LineOfSight, BlocksWhenObstacleSitsDirectlyOnTheSegment) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    const EntityRef obstacle = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 10.0F, .y = 0.0F});
    server.obstacle_store.set(obstacle,
                              line_of_sight::Obstacle{.center_x = 5.0F, .center_y = 0.0F, .radius = 1.0F});

    EXPECT_TRUE(line_of_sight::blocks_line_of_sight(
        server.ctx,
        line_of_sight::LineOfSightQuery{.obstacle = obstacle, .source = source, .target = target}));
}

TEST(LineOfSight, DoesNotBlockWhenObstacleIsFarFromTheSegment) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    const EntityRef obstacle = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 10.0F, .y = 0.0F});
    server.obstacle_store.set(obstacle,
                              line_of_sight::Obstacle{.center_x = 5.0F, .center_y = 5.0F, .radius = 1.0F});

    EXPECT_FALSE(line_of_sight::blocks_line_of_sight(
        server.ctx,
        line_of_sight::LineOfSightQuery{.obstacle = obstacle, .source = source, .target = target}));
}

TEST(LineOfSight, BlocksExactlyAtTheRadiusBoundary) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    const EntityRef obstacle = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 10.0F, .y = 0.0F});
    // Perpendicular offset from the segment exactly equals radius.
    server.obstacle_store.set(obstacle,
                              line_of_sight::Obstacle{.center_x = 5.0F, .center_y = 2.0F, .radius = 2.0F});

    EXPECT_TRUE(line_of_sight::blocks_line_of_sight(
        server.ctx,
        line_of_sight::LineOfSightQuery{.obstacle = obstacle, .source = source, .target = target}));
}

TEST(LineOfSight, DoesNotBlockJustOutsideTheRadiusBoundary) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    const EntityRef obstacle = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 10.0F, .y = 0.0F});
    server.obstacle_store.set(obstacle,
                              line_of_sight::Obstacle{.center_x = 5.0F, .center_y = 2.01F, .radius = 2.0F});

    EXPECT_FALSE(line_of_sight::blocks_line_of_sight(
        server.ctx,
        line_of_sight::LineOfSightQuery{.obstacle = obstacle, .source = source, .target = target}));
}

TEST(LineOfSight, ClosestPointClampsToSourceWhenObstacleProjectsBeforeIt) {
    // An obstacle "behind" source relative to target - the closest point on
    // the segment is source itself (t clamped to 0), not an extrapolation
    // past it, proving this checks the segment, not the infinite line
    // through it.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    const EntityRef obstacle = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 10.0F, .y = 0.0F});
    server.obstacle_store.set(obstacle,
                              line_of_sight::Obstacle{.center_x = -5.0F, .center_y = 0.0F, .radius = 1.0F});

    EXPECT_FALSE(line_of_sight::blocks_line_of_sight(
        server.ctx,
        line_of_sight::LineOfSightQuery{.obstacle = obstacle, .source = source, .target = target}));
}

TEST(LineOfSight, ClosestPointClampsToTargetWhenObstacleProjectsPastIt) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    const EntityRef obstacle = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 10.0F, .y = 0.0F});
    server.obstacle_store.set(obstacle,
                              line_of_sight::Obstacle{.center_x = 15.0F, .center_y = 0.0F, .radius = 1.0F});

    EXPECT_FALSE(line_of_sight::blocks_line_of_sight(
        server.ctx,
        line_of_sight::LineOfSightQuery{.obstacle = obstacle, .source = source, .target = target}));
}

TEST(LineOfSight, DegenerateSegmentIsTreatedAsAPointToPointCheck) {
    // Source and target at the same Position: the segment has zero length,
    // which would divide by zero in the projection formula if not guarded -
    // this proves the guard degrades gracefully to a plain point check
    // instead.
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    const EntityRef obstacle = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 3.0F, .y = 3.0F});
    server.position_store.set(target, movement::Position{.x = 3.0F, .y = 3.0F});
    server.obstacle_store.set(obstacle,
                              line_of_sight::Obstacle{.center_x = 3.0F, .center_y = 3.0F, .radius = 0.5F});

    EXPECT_TRUE(line_of_sight::blocks_line_of_sight(
        server.ctx,
        line_of_sight::LineOfSightQuery{.obstacle = obstacle, .source = source, .target = target}));
}

TEST(LineOfSight, ThrowsWithoutAnObstaclePropertySeeded) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity();
    const EntityRef obstacle = server.host.create_entity(); // no Obstacle seeded
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.position_store.set(target, movement::Position{.x = 10.0F, .y = 0.0F});

    EXPECT_THROW(
        static_cast<void>(line_of_sight::blocks_line_of_sight(
            server.ctx,
            line_of_sight::LineOfSightQuery{.obstacle = obstacle, .source = source, .target = target})),
        std::logic_error);
}

TEST(LineOfSight, ThrowsWithoutPositionOnSource) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity(); // no Position seeded
    const EntityRef target = server.host.create_entity();
    const EntityRef obstacle = server.host.create_entity();
    server.position_store.set(target, movement::Position{.x = 10.0F, .y = 0.0F});
    server.obstacle_store.set(obstacle,
                              line_of_sight::Obstacle{.center_x = 5.0F, .center_y = 0.0F, .radius = 1.0F});

    EXPECT_THROW(
        static_cast<void>(line_of_sight::blocks_line_of_sight(
            server.ctx,
            line_of_sight::LineOfSightQuery{.obstacle = obstacle, .source = source, .target = target})),
        std::logic_error);
}

TEST(LineOfSight, ThrowsWithoutPositionOnTarget) {
    SimulatedHost server{/*has_authority=*/true};
    const EntityRef source = server.host.create_entity();
    const EntityRef target = server.host.create_entity(); // no Position seeded
    const EntityRef obstacle = server.host.create_entity();
    server.position_store.set(source, movement::Position{.x = 0.0F, .y = 0.0F});
    server.obstacle_store.set(obstacle,
                              line_of_sight::Obstacle{.center_x = 5.0F, .center_y = 0.0F, .radius = 1.0F});

    EXPECT_THROW(
        static_cast<void>(line_of_sight::blocks_line_of_sight(
            server.ctx,
            line_of_sight::LineOfSightQuery{.obstacle = obstacle, .source = source, .target = target})),
        std::logic_error);
}

} // namespace
} // namespace atlas::demo
