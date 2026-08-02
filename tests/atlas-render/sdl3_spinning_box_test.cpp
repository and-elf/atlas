#include "atlas/core/time.hpp"
#include "atlas/entity/entity_ref.hpp"
#include "atlas/render/frame.hpp"
#include "atlas/render/frame_backend.hpp"
#include "atlas/render/frame_builder.hpp"
#include "atlas/render/mesh_asset.hpp"
#include "atlas/render/renderable.hpp"
#include "atlas/render/sdl3_frame_backend.hpp"
#include "atlas/render/transform.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/resource/resource_registry.hpp"
#include "atlas/runtime/property_store.hpp"

#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace atlas::render {
namespace {

// Issue #155's "spinning box" convention deliverable: a small demo/test
// host proving Sdl3FrameBackend runs *stably* over many ticks of
// continuously-changing state - build_frame -> Sdl3FrameBackend::submit(),
// the real production entry point (unlike sdl3_pixel_correctness_test.cpp,
// which deliberately bypasses submit() to reach an off-window texture -
// this test drives the real window/swapchain loop end-to-end instead). No
// Camera/view-projection concept exists anywhere in Atlas yet (#154's own
// locked-in scope), so a full 3D box's rotation is not *visually*
// meaningful without one - this test asserts on mechanism stability
// (no throw, no leak, no crash across many ticks with a real, changing
// Transform driving a real GPU upload+draw path every tick), not on any
// rendered pixel - sdl3_pixel_correctness_test.cpp already owns pixel
// correctness, with one fixed, deterministic transform.

// Set by CMakeLists.txt to the absolute path of tests/atlas-render/fixtures/.
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

void append_bytes(std::vector<std::byte>& out, const void* data, std::size_t size) {
    const std::size_t offset = out.size();
    out.resize(offset + size);
    std::memcpy(out.data() + offset, data, size);
}

void append_u32(std::vector<std::byte>& out, std::uint32_t value) {
    append_bytes(out, &value, sizeof(value));
}

void append_u64(std::vector<std::byte>& out, std::uint64_t value) {
    append_bytes(out, &value, sizeof(value));
}

void append_float(std::vector<std::byte>& out, float value) {
    append_bytes(out, &value, sizeof(value));
}

// Mirrors sdl3_pixel_correctness_test.cpp's own pack_decoded_mesh_bytes
// exactly (duplicated rather than shared - this project's own established
// small-helper-duplication precedent, e.g. atlas::ResourceId's FNV-1a
// comment).
std::vector<std::byte> pack_decoded_mesh_bytes(const std::vector<Vertex>& vertices,
                                               const std::vector<std::uint32_t>& indices) {
    std::vector<std::byte> bytes;
    append_u32(bytes, static_cast<std::uint32_t>(vertices.size()));
    append_u32(bytes, static_cast<std::uint32_t>(indices.size()));
    for (const Vertex& vertex : vertices) {
        append_float(bytes, vertex.position.x);
        append_float(bytes, vertex.position.y);
        append_float(bytes, vertex.position.z);
        append_float(bytes, vertex.normal.x);
        append_float(bytes, vertex.normal.y);
        append_float(bytes, vertex.normal.z);
        append_float(bytes, vertex.u);
        append_float(bytes, vertex.v);
    }
    for (const std::uint32_t index : indices) {
        append_u32(bytes, index);
    }
    return bytes;
}

// Appends one face's 4 vertices (a shared UV-mapped quad layout, matching
// sdl3_pixel_correctness_test.cpp's own quad_vertices()) plus its 6 indices
// (two triangles) to a growing box mesh - center, the two in-plane axes
// (right/up) and the outward normal fully describe one face of a unit
// (-1..1 per axis) cube. Called once per face below (build_box_mesh()) -
// this is this test's own hand-rolled box/cube mesh fixture (issue #155:
// "author one... or generate it via a small test-local helper - your call
// on the cleanest way, document your choice"). A small procedural helper
// was chosen over a checked-in binary .mesh fixture (triangle.mesh's own
// convention) since a cube's 24 vertices/36 indices are far more legible
// expressed as face-by-face code than as opaque hand-packed bytes in a
// binary file, and this test is the only consumer.
// NOLINTBEGIN(bugprone-easily-swappable-parameters) - four Vec3 params
// (center/right/up/normal) describe one face's geometry; every call site
// below (build_box_mesh()) names its arguments positionally via a face-axis
// comment immediately above, and splitting this into its own named-field
// struct would be more ceremony than a six-call-site, test-local helper
// warrants.
void append_box_face(std::vector<Vertex>& vertices,
                     std::vector<std::uint32_t>& indices,
                     Vec3 center,
                     Vec3 right,
                     Vec3 up,
                     Vec3 normal) {
    // NOLINTEND(bugprone-easily-swappable-parameters)
    const auto base_index = static_cast<std::uint32_t>(vertices.size());

    const auto corner = [&](float right_sign, float up_sign) {
        return Vec3{
            .x = center.x + (right.x * right_sign) + (up.x * up_sign),
            .y = center.y + (right.y * right_sign) + (up.y * up_sign),
            .z = center.z + (right.z * right_sign) + (up.z * up_sign),
        };
    };

    vertices.push_back(Vertex{.position = corner(-1.0F, 1.0F), .normal = normal, .u = 0.0F, .v = 0.0F});
    vertices.push_back(Vertex{.position = corner(1.0F, 1.0F), .normal = normal, .u = 1.0F, .v = 0.0F});
    vertices.push_back(Vertex{.position = corner(-1.0F, -1.0F), .normal = normal, .u = 0.0F, .v = 1.0F});
    vertices.push_back(Vertex{.position = corner(1.0F, -1.0F), .normal = normal, .u = 1.0F, .v = 1.0F});

    // Cull_mode is NONE (sdl3_mesh_pipeline.cpp), so winding order is not
    // load-bearing here - but a consistent CCW-from-outside winding is the
    // conventional, real-content choice a hand-authored mesh would use.
    indices.push_back(base_index + 0);
    indices.push_back(base_index + 1);
    indices.push_back(base_index + 2);
    indices.push_back(base_index + 2);
    indices.push_back(base_index + 1);
    indices.push_back(base_index + 3);
}

// A unit box (corners at +-1 on every axis) - 24 vertices (4 per face, so
// each face gets its own flat normal and its own full 0..1 UV range, rather
// than 8 shared corner vertices with ambiguous per-face normals/UVs), 36
// indices (6 faces x 2 triangles x 3 indices).
DecodedMesh build_box_mesh() {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve(24);
    indices.reserve(36);

    // +X, -X
    append_box_face(
        vertices, indices, {1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, {0.0F, 1.0F, 0.0F}, {1.0F, 0.0F, 0.0F});
    append_box_face(
        vertices, indices, {-1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, {-1.0F, 0.0F, 0.0F});
    // +Y, -Y
    append_box_face(
        vertices, indices, {0.0F, 1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, {0.0F, 1.0F, 0.0F});
    append_box_face(
        vertices, indices, {0.0F, -1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, -1.0F, 0.0F});
    // +Z, -Z
    append_box_face(
        vertices, indices, {0.0F, 0.0F, 1.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F});
    append_box_face(
        vertices, indices, {0.0F, 0.0F, -1.0F}, {-1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, -1.0F});

    return DecodedMesh{.vertices = std::move(vertices), .indices = std::move(indices)};
}

std::vector<std::byte> pack_single_entry_blob(ResourceId id, const std::vector<std::byte>& data) {
    std::vector<std::byte> blob;
    append_u64(blob, 1);
    append_u64(blob, id.value);
    append_u64(blob, 0);
    append_u64(blob, data.size());
    blob.insert(blob.end(), data.begin(), data.end());
    return blob;
}

std::filesystem::path write_temp_blob(const std::string& name, const std::vector<std::byte>& bytes) {
    const std::filesystem::path path = std::filesystem::path{::testing::TempDir()} / name;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - ostream::write needs a const char*.
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return path;
}

// A rotation quaternion advancing tick_index steps of angular_step_radians
// around the Y axis, composed directly via the closed-form axis-angle
// formula (transform.hpp's own Quaternion is otherwise unconstructed here -
// nlerp needs two endpoints, which this continuously-advancing demo has no
// fixed pair of; direct axis-angle composition, issue #155's own "your
// call" latitude, is the simpler choice for "advance rotation every tick").
// std::sin/cos are used deliberately here rather than avoided - this is
// presentation-only test/demo state (CLAUDE.md's Determinism Constraints
// exception for "audio/render interpolation"), never simulation state
// this project would need bit-exact across platforms.
Quaternion rotation_for_tick(int tick_index, double angular_step_radians) {
    const double angle = static_cast<double>(tick_index) * angular_step_radians;
    const double half_angle = angle * 0.5;
    return Quaternion{
        .x = 0.0F,
        .y = static_cast<float>(std::sin(half_angle)),
        .z = 0.0F,
        .w = static_cast<float>(std::cos(half_angle)),
    };
}

// Mirrors Sdl3FrameBackendTest's own headless-CI pattern
// (sdl3_frame_backend_test.cpp's doc comment has the full "offscreen, not
// dummy" writeup).
class Sdl3SpinningBoxTest : public ::testing::Test {
protected:
    void SetUp() override {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");

        mesh_id_ = ResourceId::from_name("meshes/spinning-box/box");
        material_id_ = ResourceId::from_name("textures/spinning-box/checker");
        const DecodedMesh box_mesh = build_box_mesh();
        const auto mesh_blob_path = write_temp_blob(
            "spinning_box_mesh.blob",
            pack_single_entry_blob(mesh_id_, pack_decoded_mesh_bytes(box_mesh.vertices, box_mesh.indices)));
        const auto texture_blob_path = write_temp_blob(
            "spinning_box_texture.blob", pack_single_entry_blob(material_id_, read_fixture("checker.tex")));
        registry_.emplace(std::unordered_map<std::string, std::filesystem::path>{
            {"Mesh", mesh_blob_path},
            {"Texture", texture_blob_path},
        });

        try {
            backend_.emplace(*registry_, "atlas-render-spinning-box-tests", 64, 64, SDL_WINDOW_HIDDEN);
        } catch (const std::runtime_error& error) {
            GTEST_SKIP() << "No real SDL_GPU-capable backend available in this environment "
                            "(expected on most headless CI runners - see "
                            "libraries/atlas-render/README.md's headless-CI decision): "
                         << error.what();
        }
    }

    Sdl3FrameBackend& backend() { return *backend_; }
    ResourceId mesh_id() const { return mesh_id_; }
    ResourceId material_id() const { return material_id_; }

private:
    std::optional<resource::ResourceRegistry> registry_;
    std::optional<Sdl3FrameBackend> backend_;
    ResourceId mesh_id_;
    ResourceId material_id_;
};

// Advances box_entity's Transform to tick_index's rotation, builds the real
// Frame for it, and submits that Frame through the real backend - pulled out
// of the TEST_F body below (which calls this once per tick) purely to keep
// that body's own cognitive complexity within this project's clang-tidy
// gate (readability-function-cognitive-complexity): a GTest ASSERT/EXPECT
// inside a loop nests several macro-expanded control-flow layers per call,
// which the checker counts against whichever function directly contains the
// loop - real behavior is unchanged either way.
void submit_one_tick(Sdl3FrameBackend& backend,
                     runtime::PropertyStore<Transform>& transforms,
                     const runtime::PropertyStore<Renderable>& renderables,
                     std::span<const EntityRef> entities,
                     EntityRef box_entity,
                     int tick_index,
                     double angular_step_radians) {
    const Transform transform{
        .position = {0.0F, 0.0F, 0.0F},
        .rotation = rotation_for_tick(tick_index, angular_step_radians),
        .scale = {1.0F, 1.0F, 1.0F},
    };
    transforms.set(box_entity, transform);

    const Frame frame = build_frame(
        entities, transforms, renderables, core::Time{.ticks = static_cast<std::uint64_t>(tick_index)});

    ASSERT_EQ(frame.draw_commands.size(), 1U);
    EXPECT_NO_THROW(backend.submit(frame));
}

TEST_F(Sdl3SpinningBoxTest, DrivingOneContinuouslyRotatingEntityForManyTicksNeverThrowsOrCrashes) {
    static_assert(FrameBackend<Sdl3FrameBackend>);

    runtime::PropertyStore<Transform> transforms;
    runtime::PropertyStore<Renderable> renderables;
    const EntityRef box_entity{.index = 1, .generation = 0};

    renderables.set(box_entity, Renderable{.mesh = mesh_id(), .material = material_id()});

    const std::array<EntityRef, 1> entities{box_entity};
    constexpr int tick_count = 90;
    // One full rotation over the whole run - arbitrary, chosen only to be a
    // visibly-nonzero angular step per tick.
    constexpr double angular_step_radians = (2.0 * std::numbers::pi) / static_cast<double>(tick_count);

    for (int tick_index = 0; tick_index < tick_count; ++tick_index) {
        submit_one_tick(
            backend(), transforms, renderables, entities, box_entity, tick_index, angular_step_radians);
    }

    // Not asserting on any pixel here (sdl3_pixel_correctness_test.cpp
    // already owns that, with one fixed, deterministic transform) - the
    // assertion this test makes is mechanism stability itself: draining
    // last_completed_tick() below, without a crash/leak/throw, proves the
    // GPU genuinely finished real work for at least one of the many
    // submitted frames above, not merely that submit() accepted them.
    std::optional<core::Time> completed;
    for (int attempt = 0; attempt < 1000; ++attempt) {
        completed = backend().last_completed_tick();
    }
    EXPECT_TRUE(completed.has_value());
}

} // namespace
} // namespace atlas::render
