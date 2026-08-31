#include "orb_transport.hpp"

#include "atlas/replication/entity_ref_codec.hpp"

#include <cstdint>

namespace atlas::demo {

std::vector<std::byte> encode_move(const MoveMessage& message) {
    serialization::ByteWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(OrbMessageKind::Move));
    replication::write_entity_ref(writer, message.target);
    writer.write_f32(message.direction_x);
    writer.write_f32(message.direction_y);
    writer.write_u64(message.delta_ticks);
    return writer.bytes();
}

std::optional<MoveMessage> decode_move(std::span<const std::byte> payload) {
    serialization::ByteReader reader(payload);

    const auto kind = reader.read_u8();
    if (!kind.has_value() || *kind != static_cast<std::uint8_t>(OrbMessageKind::Move)) {
        return std::nullopt;
    }
    const auto target = replication::read_entity_ref(reader);
    const auto direction_x = reader.read_f32();
    const auto direction_y = reader.read_f32();
    const auto delta_ticks = reader.read_u64();
    if (!target.has_value() || !direction_x.has_value() || !direction_y.has_value() ||
        !delta_ticks.has_value()) {
        return std::nullopt;
    }
    return MoveMessage{
        .target = *target,
        .direction_x = *direction_x,
        .direction_y = *direction_y,
        .delta_ticks = *delta_ticks,
    };
}

std::vector<std::byte> encode_position_update(const PositionUpdateMessage& message) {
    serialization::ByteWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(OrbMessageKind::PositionUpdate));
    replication::write_entity_ref(writer, message.entity);
    writer.write_f32(message.position.x);
    writer.write_f32(message.position.y);
    return writer.bytes();
}

std::optional<PositionUpdateMessage> decode_position_update(std::span<const std::byte> payload) {
    serialization::ByteReader reader(payload);

    const auto kind = reader.read_u8();
    if (!kind.has_value() || *kind != static_cast<std::uint8_t>(OrbMessageKind::PositionUpdate)) {
        return std::nullopt;
    }
    const auto entity = replication::read_entity_ref(reader);
    const auto x = reader.read_f32();
    const auto y = reader.read_f32();
    if (!entity.has_value() || !x.has_value() || !y.has_value()) {
        return std::nullopt;
    }
    return PositionUpdateMessage{.entity = *entity, .position = movement::Position{.x = *x, .y = *y}};
}

std::filesystem::path server_socket_path() {
    return std::filesystem::temp_directory_path() / "atlas-orb-demo-server.sock";
}

std::filesystem::path client_socket_path() {
    return std::filesystem::temp_directory_path() / "atlas-orb-demo-client.sock";
}

std::filesystem::path editor_socket_path() {
    return std::filesystem::temp_directory_path() / "atlas-orb-demo-editor.sock";
}

} // namespace atlas::demo
