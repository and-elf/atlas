#include "atlas/replication/property_id_codec.hpp"

namespace atlas::replication {

void write_property_id(serialization::ByteWriter& writer, PropertyId property_id) {
    writer.write_u64(property_id.value);
}

std::optional<PropertyId> read_property_id(serialization::ByteReader& reader) noexcept {
    const auto value = reader.read_u64();
    if (!value) {
        return std::nullopt;
    }
    return PropertyId{*value};
}

} // namespace atlas::replication
