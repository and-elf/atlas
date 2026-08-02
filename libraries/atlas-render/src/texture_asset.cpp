#include "atlas/render/texture_asset.hpp"

#include <cstring>
#include <limits>

namespace atlas::render {

namespace {

constexpr std::size_t header_size = sizeof(std::uint32_t) * 2;
constexpr std::size_t bytes_per_pixel = 4;

std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

} // namespace

std::optional<DecodedTexture> decode_texture(std::span<const std::byte> bytes) {
    if (bytes.size() < header_size) {
        return std::nullopt;
    }

    const std::uint32_t width = read_u32(bytes, 0);
    const std::uint32_t height = read_u32(bytes, sizeof(std::uint32_t));

    // width and height are each u32, so their product always fits
    // std::size_t (64-bit on every deployment target this project ships
    // to) without overflowing. Multiplying that product by
    // bytes_per_pixel below can still overflow for adversarial huge
    // dimensions though, so that step is guarded explicitly via a
    // division-based comparison rather than multiplying directly and
    // letting it silently wrap around into a small, incorrectly
    // "valid"-looking value.
    const std::size_t pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    constexpr std::size_t max_pixel_count =
        (std::numeric_limits<std::size_t>::max() - header_size) / bytes_per_pixel;
    if (pixel_count > max_pixel_count) {
        return std::nullopt;
    }

    const std::size_t needed = header_size + (pixel_count * bytes_per_pixel);
    if (bytes.size() < needed) {
        return std::nullopt; // truncated/malformed - declared dimensions run past the end of `bytes`
    }

    DecodedTexture texture;
    texture.width = width;
    texture.height = height;
    texture.pixels.assign(bytes.begin() + static_cast<std::ptrdiff_t>(header_size),
                          bytes.begin() + static_cast<std::ptrdiff_t>(needed));
    return texture;
}

} // namespace atlas::render
