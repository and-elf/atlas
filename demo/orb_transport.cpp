#include "orb_transport.hpp"

#include "atlas/replication/entity_ref_codec.hpp"
#include "atlas/replication/property_codec.hpp"
#include "atlas/replication/property_id.hpp"
#include "atlas/replication/property_id_codec.hpp"

namespace atlas::demo {

namespace {

// constexpr (PropertyId::from_name supports it) - compile-time evaluated,
// so there is no runtime static-initialization order/exception concern at
// all, not just an unlikely one.
constexpr PropertyId kMoveId = PropertyId::from_name("Move");
constexpr PropertyId kPositionId = PropertyId::from_name("Position");
constexpr PropertyId kRenderableId = PropertyId::from_name("Renderable");

} // namespace

std::vector<std::byte> encode_move(const movement::Move& message) {
    serialization::ByteWriter writer;
    replication::write_property_id(writer, kMoveId);
    replication::write_property_fields(writer, message);
    return writer.bytes();
}

std::optional<movement::Move> decode_move(std::span<const std::byte> payload) {
    serialization::ByteReader reader(payload);
    const auto id = replication::read_property_id(reader);
    if (!id.has_value() || *id != kMoveId) {
        return std::nullopt;
    }
    return replication::read_property_fields<movement::Move>(reader);
}

std::vector<std::byte> encode_position(const PositionMessage& message) {
    serialization::ByteWriter writer;
    replication::write_property_id(writer, kPositionId);
    replication::write_entity_ref(writer, message.entity);
    replication::write_property_fields(writer, message.position);
    return writer.bytes();
}

std::optional<PositionMessage> decode_position(std::span<const std::byte> payload) {
    serialization::ByteReader reader(payload);
    const auto id = replication::read_property_id(reader);
    if (!id.has_value() || *id != kPositionId) {
        return std::nullopt;
    }
    const auto entity = replication::read_entity_ref(reader);
    if (!entity.has_value()) {
        return std::nullopt;
    }
    const auto position = replication::read_property_fields<movement::Position>(reader);
    if (!position.has_value()) {
        return std::nullopt;
    }
    return PositionMessage{.entity = *entity, .position = *position};
}

std::vector<std::byte> encode_renderable(const RenderableMessage& message) {
    serialization::ByteWriter writer;
    replication::write_property_id(writer, kRenderableId);
    replication::write_entity_ref(writer, message.entity);
    replication::write_property_fields(writer, message.renderable);
    return writer.bytes();
}

std::optional<RenderableMessage> decode_renderable(std::span<const std::byte> payload) {
    serialization::ByteReader reader(payload);
    const auto id = replication::read_property_id(reader);
    if (!id.has_value() || *id != kRenderableId) {
        return std::nullopt;
    }
    const auto entity = replication::read_entity_ref(reader);
    if (!entity.has_value()) {
        return std::nullopt;
    }
    const auto renderable = replication::read_property_fields<render::Renderable>(reader);
    if (!renderable.has_value()) {
        return std::nullopt;
    }
    return RenderableMessage{.entity = *entity, .renderable = *renderable};
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
