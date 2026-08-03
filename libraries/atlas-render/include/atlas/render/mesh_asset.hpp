#pragma once

#include "atlas/render/transform.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace atlas::render {

// One decoded mesh vertex: position, normal, and texture coordinate, all
// plain floats - the conventional GPU vertex-attribute precision
// (core::Vec3, atlas/core/vec3.hpp, is already float for the same reason).
// A basic aggregate (rule of zero): no invariant beyond ordinary value
// semantics.
struct Vertex {
    core::Vec3 position;
    core::Vec3 normal;
    float u = 0.0F;
    float v = 0.0F;
};

// The output of decode_mesh: a GPU-upload-ready vertex/index buffer pair,
// decoded from this library's own minimal binary mesh format (documented
// on decode_mesh below). A basic aggregate (rule of zero): no invariant
// beyond ordinary value semantics.
struct DecodedMesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

// Decodes `bytes` (the kind of raw bytes atlas::resource::ResourceRegistry::
// resolve() produces) against this project's own minimal, hand-rolled mesh
// format:
//
//   u32 vertex_count
//   u32 index_count
//   vertex_count x { float px, py, pz; float nx, ny, nz; float u, v; }  -- 32 bytes each
//   index_count  x { u32 index }
//
// Integers/floats are read host-native (little-endian on every deployment
// target this project ships to - Debian 13 x86-64, macOS ARM, Windows
// x86-64 - stated explicitly rather than silently assumed, mirroring
// atlas::rcc::pack_resource_blob's own identical assumption, see
// tools/atlas-rcc/include/atlas/rcc/resource_blob.hpp).
//
// A hand-rolled format rather than a third-party asset importer (Assimp,
// tinyobjloader, ...) - mirroring this project's existing WAV-over-a-real-
// audio-library precedent (libraries/atlas-audio/README.md, issue #55):
// the layout above is a fixed header plus two flat arrays, trivial enough
// that a third-party dependency would not earn its keep.
//
// Returns std::nullopt for any malformed or truncated input (too short a
// header, or a declared vertex/index count whose data would run past the
// end of `bytes`) rather than throwing - matching ResourceRegistry::
// resolve()'s own three-way status: a corrupted asset is an ordinary
// runtime condition a host observes, distinct from the parse-time
// exceptions atlas-cgen/atlas-rcc throw for malformed *build-time* input.
[[nodiscard]] std::optional<DecodedMesh> decode_mesh(std::span<const std::byte> bytes);

} // namespace atlas::render
