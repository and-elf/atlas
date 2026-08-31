#pragma once

// Wire protocol for the orb demo's three separate processes (issue #278,
// building on #277's process split). A hand-written, tag-prefixed codec
// rather than atlas-replication's generic write_property_fields/
// read_property_fields (property_codec.hpp): that generic path is built on
// atlas::reflection::for_each_field, which is only generated for a
// capability's declared `properties:` block (atlas-refl) - movement::Move is
// a `requests:` entry, with no such generated FieldVisitable specialization,
// so it is encoded by hand here instead, the same "manual codec, same
// atlas-serialization primitives underneath" precedent this library's own
// README documents for health::write_health/read_health predating the
// generic path.
//
// Each payload starts with a one-byte OrbMessageKind tag (issue #6's uniform
// "self-describing wire tuple" convention, mirroring PropertyId's own role)
// so a single socket could in principle carry either message kind, even
// though today Move only ever flows editor->server and PositionUpdate only
// ever flows server->client/editor.

#include "atlas/entity/entity_ref.hpp"
#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include "movement/movement.hpp"

namespace atlas::demo {

enum class OrbMessageKind : std::uint8_t {
    Move = 1,
    PositionUpdate = 2,
};

// A decoded movement::Move, minus the delta_ticks field - the wire always
// carries delta_ticks=1 (editor-host sends one message per tick, matching
// its own local tick rate) so there is nothing meaningful to negotiate here;
// kept anyway as an explicit field rather than hardcoding 1 at the decode
// site, in case a future round batches more than one tick per message.
struct MoveMessage {
    EntityRef target;
    float direction_x = 0.0F;
    float direction_y = 0.0F;
    std::uint64_t delta_ticks = 0;
};

struct PositionUpdateMessage {
    EntityRef entity;
    movement::Position position;
};

[[nodiscard]] std::vector<std::byte> encode_move(const MoveMessage& message);
[[nodiscard]] std::optional<MoveMessage> decode_move(std::span<const std::byte> payload);

[[nodiscard]] std::vector<std::byte> encode_position_update(const PositionUpdateMessage& message);
[[nodiscard]] std::optional<PositionUpdateMessage> decode_position_update(std::span<const std::byte> payload);

// Well-known bind paths (issue #278's own deliberately simple scope: no
// handshake/session-id layer, spec §215 phases 3-4, still deferred) - each
// of the three orb-demo processes binds its own fixed path under the OS
// temp directory, so server-host can unconditionally send a PositionUpdate
// to client-host's and editor-host's paths every tick without either of
// them ever having to announce themselves first.
[[nodiscard]] std::filesystem::path server_socket_path();
[[nodiscard]] std::filesystem::path client_socket_path();
[[nodiscard]] std::filesystem::path editor_socket_path();

} // namespace atlas::demo
