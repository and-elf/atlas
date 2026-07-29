#pragma once

#include "atlas/entity/entity_ref.hpp"
#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"

#include <optional>

namespace atlas::replication {

// Wire format: index then generation, each an explicit little-endian u32 (via
// atlas-serialization), never a memcpy of EntityRef's native layout — the
// same determinism rationale ByteWriter/ByteReader already document (spec §4:
// bit-exact results across machines rules out a struct-layout-dependent
// encoding). This is the minimal vocabulary this library needs before any
// property/state replication can reference an entity on the wire at all
// (spec §6, Server Authority: "Replication distributes observable state from
// authoritative hosts" — naming the entity is the prerequisite).
void write_entity_ref(serialization::ByteWriter& writer, EntityRef entity_ref);

// Fails explicitly (std::nullopt) if either field is truncated, mirroring
// ByteReader's own past-the-end behavior rather than reading a partially
// decoded EntityRef.
[[nodiscard]] std::optional<EntityRef> read_entity_ref(serialization::ByteReader& reader) noexcept;

} // namespace atlas::replication
