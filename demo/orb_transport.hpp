#pragma once

// Wire protocol for the orb demo's three separate processes (issue #278,
// building on #277's process split). Uses atlas-replication's generic,
// reflection-driven property codec (write_property_id/write_property_fields,
// property_codec.hpp/property_id_codec.hpp, issue #18) instead of a
// hand-written per-field codec: movement::Move/movement::Position/
// render::Renderable are all ordinary aggregates whose fields are either
// fixed-width primitives or themselves FieldVisitable aggregates
// (EntityRef, ResourceId) - they already satisfy
// atlas::reflection::FieldVisitable with no code generation required at
// all. FieldVisitable is a generic C++23 structural concept (for_each_field
// walks the real compiled struct via a structured-binding dispatch table),
// not something atlas-cgen/atlas-refl emit per type - atlas-refl only emits
// *descriptive* metadata (field name/type-spelling strings for generic
// tooling display), never generated (de)serialization code, so there is
// nothing to wait on either way. This mirrors
// demo/tests/simulated_host.hpp's own replicate_health_to precedent
// exactly.
//
// Each payload starts with a PropertyId tag naming which struct follows
// (the same self-describing-wire-tuple convention every other
// atlas-replication codec already uses), then - for Position/Renderable,
// which don't carry their own identity - an EntityRef, then the struct's
// own fields via write_property_fields. Move already carries its own
// `target` field, so no separate EntityRef is written for it.

#include "atlas/entity/entity_ref.hpp"
#include "atlas/render/renderable.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include "movement/movement.hpp"

namespace atlas::demo {

[[nodiscard]] std::vector<std::byte> encode_move(const movement::Move& message);
[[nodiscard]] std::optional<movement::Move> decode_move(std::span<const std::byte> payload);

struct PositionMessage {
    EntityRef entity;
    movement::Position position;
};

[[nodiscard]] std::vector<std::byte> encode_position(const PositionMessage& message);
[[nodiscard]] std::optional<PositionMessage> decode_position(std::span<const std::byte> payload);

// Carries the target's full desired Renderable (mesh + material), not just
// material - the same message shape serves both directions: editor-host ->
// server-host ("set my Renderable to this") and server-host ->
// client-host/editor-host (the broadcast of the orb's current Renderable) -
// one generic-coded type instead of two direction-specific ones.
struct RenderableMessage {
    EntityRef entity;
    render::Renderable renderable;
};

[[nodiscard]] std::vector<std::byte> encode_renderable(const RenderableMessage& message);
[[nodiscard]] std::optional<RenderableMessage> decode_renderable(std::span<const std::byte> payload);

// Well-known bind paths (issue #278's own deliberately simple scope: no
// handshake/session-id layer, spec §215 phases 3-4, still deferred) - each
// of the three orb-demo processes binds its own fixed path under the OS
// temp directory, so server-host can unconditionally send a broadcast to
// client-host's and editor-host's paths every tick without either of them
// ever having to announce themselves first.
[[nodiscard]] std::filesystem::path server_socket_path();
[[nodiscard]] std::filesystem::path client_socket_path();
[[nodiscard]] std::filesystem::path editor_socket_path();

} // namespace atlas::demo
