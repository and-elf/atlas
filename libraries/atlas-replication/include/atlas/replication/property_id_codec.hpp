#pragma once

#include "atlas/replication/property_id.hpp"
#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"

#include <optional>

namespace atlas::replication {

// Wire format: value as a single explicit little-endian u64 (via
// atlas-serialization), never a memcpy of PropertyId's native layout - the
// same determinism rationale ByteWriter/ByteReader already document (spec
// §4). Mirrors resource_id_codec.hpp exactly, since PropertyId is a single-
// u64 vocabulary type of the same shape as ResourceId.
void write_property_id(serialization::ByteWriter& writer, PropertyId property_id);

// Fails explicitly (std::nullopt) if the field is truncated, mirroring
// ByteReader's own past-the-end behavior rather than reading a partially
// decoded PropertyId.
[[nodiscard]] std::optional<PropertyId> read_property_id(serialization::ByteReader& reader) noexcept;

} // namespace atlas::replication
