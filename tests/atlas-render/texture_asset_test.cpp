#include "atlas/render/texture_asset.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <vector>

namespace atlas::render {
namespace {

// Set by CMakeLists.txt to the absolute path of tests/atlas-render/fixtures/
// - real fixture files on disk, matching mesh_asset_test.cpp's own
// precedent and tests/atlas-resource/resource_registry_test.cpp's original.
constexpr std::string_view fixtures_dir = ATLAS_RENDER_TEST_FIXTURES_DIR;

std::vector<std::byte> read_fixture(const std::string& name) {
    const std::filesystem::path path = std::filesystem::path{fixtures_dir} / name;
    std::ifstream file(path, std::ios::binary);
    std::vector<char> raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    std::vector<std::byte> bytes(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        bytes[i] = static_cast<std::byte>(raw[i]);
    }
    return bytes;
}

TEST(DecodeTexture, WellFormedTextureDecodesToExactlyTheExpectedPixelsAndDimensions) {
    const std::vector<std::byte> bytes = read_fixture("checker.tex");

    const std::optional<DecodedTexture> texture = decode_texture(bytes);

    ASSERT_TRUE(texture.has_value());
    EXPECT_EQ(texture->width, 2U);
    EXPECT_EQ(texture->height, 2U);
    ASSERT_EQ(texture->pixels.size(), 16U); // 2 * 2 * 4 bytes-per-pixel

    // Top-left pixel: opaque red.
    EXPECT_EQ(texture->pixels[0], std::byte{255});
    EXPECT_EQ(texture->pixels[1], std::byte{0});
    EXPECT_EQ(texture->pixels[2], std::byte{0});
    EXPECT_EQ(texture->pixels[3], std::byte{255});

    // Bottom-right pixel: translucent yellow.
    EXPECT_EQ(texture->pixels[12], std::byte{255});
    EXPECT_EQ(texture->pixels[13], std::byte{255});
    EXPECT_EQ(texture->pixels[14], std::byte{0});
    EXPECT_EQ(texture->pixels[15], std::byte{128});
}

TEST(DecodeTexture, RepeatedDecodesOfTheSameBytesProduceBitIdenticalOutput) {
    const std::vector<std::byte> bytes = read_fixture("checker.tex");

    const std::optional<DecodedTexture> first = decode_texture(bytes);
    const std::optional<DecodedTexture> second = decode_texture(bytes);

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first->width, second->width);
    EXPECT_EQ(first->height, second->height);
    EXPECT_EQ(first->pixels, second->pixels);
}

TEST(DecodeTexture, TruncatedPixelDataFailsToDecode) {
    const std::vector<std::byte> bytes = read_fixture("truncated_checker.tex");

    const std::optional<DecodedTexture> texture = decode_texture(bytes);

    EXPECT_FALSE(texture.has_value());
}

TEST(DecodeTexture, TruncatedHeaderFailsToDecode) {
    const std::vector<std::byte> bytes = read_fixture("truncated_header.tex");

    const std::optional<DecodedTexture> texture = decode_texture(bytes);

    EXPECT_FALSE(texture.has_value());
}

TEST(DecodeTexture, EmptyInputFailsToDecodeRatherThanReadingOutOfBounds) {
    const std::optional<DecodedTexture> texture = decode_texture(std::span<const std::byte>{});

    EXPECT_FALSE(texture.has_value());
}

TEST(DecodeTexture, AdversarialDimensionsThatWouldOverflowTheSizeComputationFailToDecode) {
    // huge_dimensions.tex declares width = height = 0xFFFFFFFF, whose
    // pixel-byte count (width * height * 4) overflows std::size_t and would
    // wrap around to a small, incorrectly "valid"-looking value if computed
    // naively - decode_texture must reject this via its overflow-safe
    // check rather than proceeding to read out of bounds.
    const std::vector<std::byte> bytes = read_fixture("huge_dimensions.tex");

    const std::optional<DecodedTexture> texture = decode_texture(bytes);

    EXPECT_FALSE(texture.has_value());
}

TEST(DecodeTexture, ZeroWidthOrHeightDecodesToAnEmptyPixelBuffer) {
    // Header-only bytes declaring a zero dimension is well-formed per the
    // format (texture_asset.hpp): an empty texture, not a malformed one.
    const std::vector<std::byte> bytes(8, std::byte{0});

    const std::optional<DecodedTexture> texture = decode_texture(bytes);

    ASSERT_TRUE(texture.has_value());
    EXPECT_EQ(texture->width, 0U);
    EXPECT_EQ(texture->height, 0U);
    EXPECT_TRUE(texture->pixels.empty());
}

} // namespace
} // namespace atlas::render
