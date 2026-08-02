#include "atlas/render/mesh_asset.hpp"

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
// - real fixture files on disk, not inline byte arrays, per CLAUDE.md's
// "test via a minimal test host, not by mocking behavior" rule and
// tests/atlas-resource/resource_registry_test.cpp's own precedent.
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

TEST(DecodeMesh, WellFormedTriangleDecodesToExactlyTheExpectedVerticesAndIndices) {
    const std::vector<std::byte> bytes = read_fixture("triangle.mesh");

    const std::optional<DecodedMesh> mesh = decode_mesh(bytes);

    ASSERT_TRUE(mesh.has_value());
    ASSERT_EQ(mesh->vertices.size(), 3U);
    ASSERT_EQ(mesh->indices.size(), 3U);

    EXPECT_FLOAT_EQ(mesh->vertices[0].position.x, 0.0F);
    EXPECT_FLOAT_EQ(mesh->vertices[0].position.y, 0.0F);
    EXPECT_FLOAT_EQ(mesh->vertices[0].position.z, 0.0F);
    EXPECT_FLOAT_EQ(mesh->vertices[0].normal.z, 1.0F);
    EXPECT_FLOAT_EQ(mesh->vertices[0].u, 0.0F);
    EXPECT_FLOAT_EQ(mesh->vertices[0].v, 0.0F);

    EXPECT_FLOAT_EQ(mesh->vertices[1].position.x, 1.0F);
    EXPECT_FLOAT_EQ(mesh->vertices[1].u, 1.0F);

    EXPECT_FLOAT_EQ(mesh->vertices[2].position.y, 1.0F);
    EXPECT_FLOAT_EQ(mesh->vertices[2].v, 1.0F);

    EXPECT_EQ(mesh->indices[0], 0U);
    EXPECT_EQ(mesh->indices[1], 1U);
    EXPECT_EQ(mesh->indices[2], 2U);
}

// Field-by-field, unrolled rather than looped, matching this library's
// existing determinism-test style (frame_builder_test.cpp's
// BuildFrame.RepeatedCallsWithIdenticalInputProduceIdenticalOutput) - a
// loop over EXPECT_EQ macros here pushes TestBody's cognitive complexity
// past this project's clang-tidy threshold for no real gain, since
// triangle.mesh's vertex count is fixed and known.
TEST(DecodeMesh, RepeatedDecodesOfTheSameBytesProduceBitIdenticalOutput) {
    const std::vector<std::byte> bytes = read_fixture("triangle.mesh");

    const std::optional<DecodedMesh> first = decode_mesh(bytes);
    const std::optional<DecodedMesh> second = decode_mesh(bytes);

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_EQ(first->vertices.size(), second->vertices.size());
    EXPECT_EQ(first->indices, second->indices);
    EXPECT_EQ(first->vertices[0].position.x, second->vertices[0].position.x);
    EXPECT_EQ(first->vertices[1].position.x, second->vertices[1].position.x);
    EXPECT_EQ(first->vertices[2].position.y, second->vertices[2].position.y);
}

TEST(DecodeMesh, TruncatedVertexOrIndexDataFailsToDecode) {
    const std::vector<std::byte> bytes = read_fixture("truncated_triangle.mesh");

    const std::optional<DecodedMesh> mesh = decode_mesh(bytes);

    EXPECT_FALSE(mesh.has_value());
}

TEST(DecodeMesh, TruncatedHeaderFailsToDecode) {
    const std::vector<std::byte> bytes = read_fixture("truncated_header.mesh");

    const std::optional<DecodedMesh> mesh = decode_mesh(bytes);

    EXPECT_FALSE(mesh.has_value());
}

TEST(DecodeMesh, EmptyInputFailsToDecodeRatherThanReadingOutOfBounds) {
    const std::optional<DecodedMesh> mesh = decode_mesh(std::span<const std::byte>{});

    EXPECT_FALSE(mesh.has_value());
}

TEST(DecodeMesh, ZeroVertexAndIndexCountsDecodeToAnEmptyMesh) {
    // Header-only bytes declaring zero vertices/zero indices is well-formed
    // per the format (decode_mesh.hpp): an empty mesh, not a malformed one.
    const std::vector<std::byte> bytes(8, std::byte{0});

    const std::optional<DecodedMesh> mesh = decode_mesh(bytes);

    ASSERT_TRUE(mesh.has_value());
    EXPECT_TRUE(mesh->vertices.empty());
    EXPECT_TRUE(mesh->indices.empty());
}

} // namespace
} // namespace atlas::render
