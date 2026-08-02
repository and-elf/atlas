#include "atlas/render/mesh_asset.hpp"

#include <cstring>

namespace atlas::render {

namespace {

constexpr std::size_t header_size = sizeof(std::uint32_t) * 2;
constexpr std::size_t vertex_size = sizeof(float) * 8; // px,py,pz, nx,ny,nz, u,v
constexpr std::size_t index_size = sizeof(std::uint32_t);

std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

float read_f32(std::span<const std::byte> bytes, std::size_t offset) {
    float value = 0.0F;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

} // namespace

std::optional<DecodedMesh> decode_mesh(std::span<const std::byte> bytes) {
    if (bytes.size() < header_size) {
        return std::nullopt;
    }

    const std::uint32_t vertex_count = read_u32(bytes, 0);
    const std::uint32_t index_count = read_u32(bytes, sizeof(std::uint32_t));

    // Both counts are u32 and each only ever multiplies against a small
    // fixed per-element size (32 / 4 bytes) rather than against each other,
    // so this sum can never overflow std::size_t (64-bit on every
    // deployment target this project ships to) the way decode_texture's
    // width * height product can - no overflow guard needed here.
    const std::size_t vertex_bytes = static_cast<std::size_t>(vertex_count) * vertex_size;
    const std::size_t index_bytes = static_cast<std::size_t>(index_count) * index_size;
    const std::size_t needed = header_size + vertex_bytes + index_bytes;
    if (bytes.size() < needed) {
        return std::nullopt; // truncated/malformed - declared counts run past the end of `bytes`
    }

    DecodedMesh mesh;
    mesh.vertices.reserve(vertex_count);
    std::size_t offset = header_size;
    for (std::uint32_t i = 0; i < vertex_count; ++i) {
        mesh.vertices.push_back(Vertex{
            .position = {read_f32(bytes, offset), read_f32(bytes, offset + 4), read_f32(bytes, offset + 8)},
            .normal = {read_f32(bytes, offset + 12),
                       read_f32(bytes, offset + 16),
                       read_f32(bytes, offset + 20)},
            .u = read_f32(bytes, offset + 24),
            .v = read_f32(bytes, offset + 28),
        });
        offset += vertex_size;
    }

    mesh.indices.reserve(index_count);
    for (std::uint32_t i = 0; i < index_count; ++i) {
        mesh.indices.push_back(read_u32(bytes, offset));
        offset += index_size;
    }

    return mesh;
}

} // namespace atlas::render
