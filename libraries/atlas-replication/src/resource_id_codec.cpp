#include "atlas/replication/resource_id_codec.hpp"

namespace atlas::replication {

void write_resource_id(serialization::ByteWriter& writer, ResourceId resource_id) {
    writer.write_u64(resource_id.value);
}

std::optional<ResourceId> read_resource_id(serialization::ByteReader& reader) noexcept {
    const auto value = reader.read_u64();
    if (!value) {
        return std::nullopt;
    }

    return ResourceId{.value = *value};
}

} // namespace atlas::replication
