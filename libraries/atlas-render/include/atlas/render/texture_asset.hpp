#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace atlas::render {

// The output of decode_texture: raw, uncompressed 8-bit-per-channel RGBA
// pixel data plus its dimensions - a GPU-upload-ready 2D texture source,
// decoded from this library's own minimal binary texture format
// (documented on decode_texture below). A basic aggregate (rule of zero):
// no invariant beyond ordinary value semantics.
struct DecodedTexture {
    std::vector<std::byte> pixels;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// Decodes `bytes` (the kind of raw bytes atlas::resource::ResourceRegistry::
// resolve() produces) against this project's own minimal, hand-rolled
// texture format:
//
//   u32 width
//   u32 height
//   width * height x { u8 r, g, b, a }   -- row-major, top row first, no padding
//
// Deliberately raw/uncompressed RGBA8 rather than a real image format
// (PNG/DDS/a compressed block format) - mirroring decode_mesh's own
// reasoning (mesh_asset.hpp) and this project's WAV-over-a-real-audio-
// library precedent (libraries/atlas-audio/README.md, issue #55): a fixed
// header plus one flat pixel array needs no third-party dependency to
// decode.
//
// Returns std::nullopt for any malformed or truncated input, including a
// declared width/height whose pixel data would run past the end of
// `bytes` - checked via an overflow-safe comparison rather than
// multiplying width * height * 4 directly, since two adversarial u32
// dimensions can make that product overflow std::size_t and wrap back
// around to a small, incorrectly "valid"-looking value.
[[nodiscard]] std::optional<DecodedTexture> decode_texture(std::span<const std::byte> bytes);

} // namespace atlas::render
