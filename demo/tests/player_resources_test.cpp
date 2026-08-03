// issue #200: proves the whole authored-asset pipeline for real - the
// checked-in demo/resources/Mesh.blob/Texture.blob (packed via atlas-rcc's
// CLI from demo/resources/manifest.yaml + demo/resources/raw/player.mesh/
// player.tex) actually resolve through a real ResourceRegistry and decode
// into the exact placeholder geometry/pixels authored - mirroring
// demo/tests/door_test.cpp's own
// OpeningTheDoorResourceResolvesToTheRealSoundBytes precedent, for Mesh/
// Texture instead of Sound.
#include "atlas/render/mesh_asset.hpp"
#include "atlas/render/texture_asset.hpp"
#include "atlas/resource/resource_registry.hpp"

#include <cstddef>
#include <filesystem>
#include <gtest/gtest.h>
#include <string_view>

#include "player_resources.hpp"

namespace atlas::demo {
namespace {

constexpr std::string_view resources_dir = DEMO_RESOURCES_DIR;

resource::ResourceRegistry make_registry() {
    return resource::ResourceRegistry{{
        {"Mesh", std::filesystem::path{resources_dir} / "Mesh.blob"},
        {"Texture", std::filesystem::path{resources_dir} / "Texture.blob"},
    }};
}

TEST(PlayerResources, TheMeshResourceResolvesAndDecodesIntoTheAuthoredQuad) {
    resource::ResourceRegistry registry = make_registry();
    const auto mesh_id = ResourceId::from_name(kPlayerMeshResourceName);

    const resource::Resolution resolution = registry.resolve("Mesh", mesh_id);
    ASSERT_EQ(resolution.status, resource::ResolutionStatus::Resolved);

    const auto decoded = render::decode_mesh(resolution.bytes);
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->vertices.size(), 4U);
    ASSERT_EQ(decoded->indices.size(), 6U);

    // The authored quad's first vertex: position (-0.5, 0, -0.5), normal
    // (0, 1, 0), uv (0, 0) - see the generation script referenced in #200's
    // PR description.
    EXPECT_FLOAT_EQ(decoded->vertices[0].position.x, -0.5F);
    EXPECT_FLOAT_EQ(decoded->vertices[0].position.y, 0.0F);
    EXPECT_FLOAT_EQ(decoded->vertices[0].position.z, -0.5F);
    EXPECT_FLOAT_EQ(decoded->vertices[0].normal.y, 1.0F);
    EXPECT_EQ(decoded->indices[0], 0U);
    EXPECT_EQ(decoded->indices[5], 3U);
}

TEST(PlayerResources, TheTextureResourceResolvesAndDecodesIntoTheAuthoredCheckerPattern) {
    resource::ResourceRegistry registry = make_registry();
    const auto texture_id = ResourceId::from_name(kPlayerTextureResourceName);

    const resource::Resolution resolution = registry.resolve("Texture", texture_id);
    ASSERT_EQ(resolution.status, resource::ResolutionStatus::Resolved);

    const auto decoded = render::decode_texture(resolution.bytes);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->width, 2U);
    EXPECT_EQ(decoded->height, 2U);
    ASSERT_EQ(decoded->pixels.size(), 16U); // 2x2 RGBA8

    // Top-left pixel: opaque white.
    EXPECT_EQ(static_cast<unsigned char>(decoded->pixels[0]), 255U);
    EXPECT_EQ(static_cast<unsigned char>(decoded->pixels[1]), 255U);
    EXPECT_EQ(static_cast<unsigned char>(decoded->pixels[2]), 255U);
    EXPECT_EQ(static_cast<unsigned char>(decoded->pixels[3]), 255U);
}

TEST(PlayerResources, AnUnrelatedResourceIdDoesNotResolve) {
    resource::ResourceRegistry registry = make_registry();
    const auto unrelated_id = ResourceId::from_name("characters/not-the-player/mesh");

    const resource::Resolution resolution = registry.resolve("Mesh", unrelated_id);
    EXPECT_EQ(resolution.status, resource::ResolutionStatus::Unresolved);
}

} // namespace
} // namespace atlas::demo
