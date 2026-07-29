#pragma once

#include "atlas/resource/resource_id.hpp"
#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"

#include <optional>

namespace atlas::replication {

// Wire format: value as a single explicit little-endian u64 (via
// atlas-serialization), never a memcpy of ResourceId's native layout — the
// same determinism rationale ByteWriter/ByteReader already document (spec §4:
// bit-exact results across machines rules out a struct-layout-dependent
// encoding). Unlike EntityRef this is a single field, so no further framing
// is needed beyond the one write_u64/read_u64 call.
void write_resource_id(serialization::ByteWriter& writer, ResourceId resource_id);

// Fails explicitly (std::nullopt) if the field is truncated, mirroring
// ByteReader's own past-the-end behavior rather than reading a partially
// decoded ResourceId.
[[nodiscard]] std::optional<ResourceId> read_resource_id(serialization::ByteReader& reader) noexcept;

} // namespace atlas::replication
