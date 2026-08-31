#include <gtest/gtest.h>

#include "orb_transport.hpp"

namespace atlas::demo {
namespace {

TEST(OrbTransport, EncodeDecodeMoveRoundTrips) {
    const MoveMessage original{
        .target = EntityRef{.index = 3, .generation = 7},
        .direction_x = 0.5F,
        .direction_y = -0.25F,
        .delta_ticks = 1,
    };

    const auto decoded = decode_move(encode_move(original));

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->target, original.target);
    EXPECT_FLOAT_EQ(decoded->direction_x, original.direction_x);
    EXPECT_FLOAT_EQ(decoded->direction_y, original.direction_y);
    EXPECT_EQ(decoded->delta_ticks, original.delta_ticks);
}

TEST(OrbTransport, DecodeMoveRejectsAPositionUpdatePayload) {
    const auto payload = encode_position_update(PositionUpdateMessage{
        .entity = EntityRef{.index = 1, .generation = 1},
        .position = movement::Position{.x = 0.0F, .y = 0.0F},
    });

    EXPECT_FALSE(decode_move(payload).has_value());
}

TEST(OrbTransport, DecodeMoveRejectsATruncatedPayload) {
    auto payload = encode_move(MoveMessage{
        .target = EntityRef{.index = 1, .generation = 1},
        .direction_x = 1.0F,
        .direction_y = 0.0F,
        .delta_ticks = 1,
    });
    payload.pop_back();

    EXPECT_FALSE(decode_move(payload).has_value());
}

TEST(OrbTransport, EncodeDecodePositionUpdateRoundTrips) {
    const PositionUpdateMessage original{
        .entity = EntityRef{.index = 2, .generation = 5},
        .position = movement::Position{.x = 1.5F, .y = -3.25F},
    };

    const auto decoded = decode_position_update(encode_position_update(original));

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->entity, original.entity);
    EXPECT_FLOAT_EQ(decoded->position.x, original.position.x);
    EXPECT_FLOAT_EQ(decoded->position.y, original.position.y);
}

TEST(OrbTransport, DecodePositionUpdateRejectsAMovePayload) {
    const auto payload = encode_move(MoveMessage{
        .target = EntityRef{.index = 1, .generation = 1},
        .direction_x = 1.0F,
        .direction_y = 0.0F,
        .delta_ticks = 1,
    });

    EXPECT_FALSE(decode_position_update(payload).has_value());
}

TEST(OrbTransport, DecodePositionUpdateRejectsATruncatedPayload) {
    auto payload = encode_position_update(PositionUpdateMessage{
        .entity = EntityRef{.index = 1, .generation = 1},
        .position = movement::Position{.x = 0.0F, .y = 0.0F},
    });
    payload.pop_back();

    EXPECT_FALSE(decode_position_update(payload).has_value());
}

TEST(OrbTransport, SocketPathsAreDistinctAndStableAcrossCalls) {
    EXPECT_EQ(server_socket_path(), server_socket_path());
    EXPECT_EQ(client_socket_path(), client_socket_path());
    EXPECT_EQ(editor_socket_path(), editor_socket_path());

    EXPECT_NE(server_socket_path(), client_socket_path());
    EXPECT_NE(server_socket_path(), editor_socket_path());
    EXPECT_NE(client_socket_path(), editor_socket_path());
}

} // namespace
} // namespace atlas::demo
