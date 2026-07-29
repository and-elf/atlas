#include "atlas/replication/entity_ref_codec.hpp"

namespace atlas::replication {

void write_entity_ref(serialization::ByteWriter& writer, EntityRef entity_ref) {
    writer.write_u32(entity_ref.index);
    writer.write_u32(entity_ref.generation);
}

std::optional<EntityRef> read_entity_ref(serialization::ByteReader& reader) noexcept {
    const auto index = reader.read_u32();
    if (!index) {
        return std::nullopt;
    }

    const auto generation = reader.read_u32();
    if (!generation) {
        return std::nullopt;
    }

    return EntityRef{.index = *index, .generation = *generation};
}

} // namespace atlas::replication
