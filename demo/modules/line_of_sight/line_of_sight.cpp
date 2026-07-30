#include "line_of_sight.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace atlas::line_of_sight {

bool blocks_line_of_sight(Context& ctx, const LineOfSightQuery& query) {
    const auto obstacle_shape = ctx.get<Obstacle>(query.obstacle);
    if (!obstacle_shape) {
        throw std::logic_error(
            "atlas::line_of_sight::blocks_line_of_sight: obstacle has no Obstacle property");
    }

    const auto source_position = ctx.get<movement::Position>(query.source);
    if (!source_position) {
        throw std::logic_error("atlas::line_of_sight::blocks_line_of_sight: source has no Position");
    }

    const auto target_position = ctx.get<movement::Position>(query.target);
    if (!target_position) {
        throw std::logic_error("atlas::line_of_sight::blocks_line_of_sight: target has no Position");
    }

    const float segment_x = target_position->get().x - source_position->get().x;
    const float segment_y = target_position->get().y - source_position->get().y;
    const float segment_length_squared = (segment_x * segment_x) + (segment_y * segment_y);

    const float to_obstacle_x = obstacle_shape->get().center_x - source_position->get().x;
    const float to_obstacle_y = obstacle_shape->get().center_y - source_position->get().y;

    // Project the obstacle's center onto the segment, clamped to [0, 1] so
    // the closest point is never extrapolated past either endpoint - the
    // standard point-to-segment distance construction. Source and target at
    // the same Position (segment_length_squared == 0) would divide by zero
    // here; guarded by degenerating to t = 0, which makes closest below
    // equal source exactly - a plain point-to-point check, the correct
    // answer when there is no segment to project onto.
    float t = 0.0F;
    if (segment_length_squared > 0.0F) {
        t = std::clamp(
            ((to_obstacle_x * segment_x) + (to_obstacle_y * segment_y)) / segment_length_squared, 0.0F, 1.0F);
    }

    const float closest_x = source_position->get().x + (t * segment_x);
    const float closest_y = source_position->get().y + (t * segment_y);

    const float distance_x = obstacle_shape->get().center_x - closest_x;
    const float distance_y = obstacle_shape->get().center_y - closest_y;
    const float distance = std::sqrt((distance_x * distance_x) + (distance_y * distance_y));

    return distance <= obstacle_shape->get().radius;
}

} // namespace atlas::line_of_sight
