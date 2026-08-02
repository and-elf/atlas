#include "atlas/render/frame_backend.hpp"
#include "atlas/render/sdl3_frame_backend.hpp"

#include <SDL3/SDL.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace atlas::render {
namespace {

static_assert(FrameBackend<Sdl3FrameBackend>);

// Set by CMakeLists.txt to the absolute path of tests/atlas-render/fixtures/
// - real fixture files on disk (triangle.mesh, checker.tex), not mocked
// bytes, matching mesh_upload_cache_test.cpp/texture_upload_cache_test.cpp's
// own convention.
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

void append_u64(std::vector<std::byte>& out, std::uint64_t value) {
    append_bytes(out, &value, sizeof(value));
}

// Mirrors mesh_upload_cache_test.cpp/texture_upload_cache_test.cpp's own
// pack_single_entry_blob exactly.
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

// Most CI runners (this sandbox included - no /dev/dri, no Vulkan ICD
// installed) have no real GPU or display hardware, which is exactly why
// issue #148 built NullFrameBackend in the first place (see this library's
// README, "headless-CI decision"). Every fixture-based test below attempts
// *real* SDL3 window + SDL_GPU device creation in SetUp() and, on failure,
// calls GTEST_SKIP() with the thrown exception's message rather than
// treating it as a test failure - the constructor throwing
// std::runtime_error is itself the behavior under test on a machine like
// this one. SDL_HINT_VIDEO_DRIVER is forced to "dummy" first so windowing
// itself succeeds headlessly (no DISPLAY needed) - GPU device creation is
// then the part actually expected to fail here, absent a Vulkan/Metal/D3D12
// driver, giving line coverage over the real success path on any machine
// that does have one.
//
// The ResourceRegistry backing every fixture is built from real,
// well-formed mesh/texture fixture bytes (triangle.mesh, checker.tex) packed
// into a real single-entry resource blob at test-run time (mirroring
// tests/atlas-audio/decode_cache_test.cpp's own convention, issue #154's
// direct template) - a real Frame/DrawCommand submitted against this
// registry (below) exercises the full resolve -> decode -> GPU-upload ->
// draw path, not a mocked shortcut through any part of it.
class Sdl3FrameBackendTest : public ::testing::Test {
protected:
    void SetUp() override {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");

        mesh_id_ = ResourceId::from_name("meshes/sdl3-frame-backend/triangle");
        material_id_ = ResourceId::from_name("textures/sdl3-frame-backend/checker");
        const auto mesh_blob_path = write_temp_blob(
            "sdl3_frame_backend_mesh.blob", pack_single_entry_blob(mesh_id_, read_fixture("triangle.mesh")));
        const auto texture_blob_path =
            write_temp_blob("sdl3_frame_backend_texture.blob",
                            pack_single_entry_blob(material_id_, read_fixture("checker.tex")));
        registry_.emplace(std::unordered_map<std::string, std::filesystem::path>{
            {"Mesh", mesh_blob_path},
            {"Texture", texture_blob_path},
        });

        try {
            backend_.emplace(*registry_, "atlas-render-tests", 64, 64, SDL_WINDOW_HIDDEN);
        } catch (const std::runtime_error& error) {
            GTEST_SKIP() << "No real SDL_GPU-capable backend available in this environment "
                            "(expected on most headless CI runners - see "
                            "libraries/atlas-render/README.md's headless-CI decision): "
                         << error.what();
        }
    }

    // Polls last_completed_tick() up to max_attempts times, returning as
    // soon as it reports a value - never assumes instant completion (that
    // is the entire point of a real GPU fence over NullFrameBackend's
    // "instantly complete" shortcut), and never spins forever if a driver
    // somehow never signals.
    std::optional<core::Time> poll_until_completed(int max_attempts = 1000) {
        std::optional<core::Time> completed;
        for (int attempt = 0; attempt < max_attempts && !completed.has_value(); ++attempt) {
            completed = backend().last_completed_tick();
        }
        return completed;
    }

    // A protected accessor (rather than a protected data member) keeps
    // backend_ itself private, satisfying cppcoreguidelines' "no protected
    // data members" check while still giving every TEST_F body below the
    // access a GTest fixture is for.
    Sdl3FrameBackend& backend() { return *backend_; }
    ResourceId mesh_id() const { return mesh_id_; }
    ResourceId material_id() const { return material_id_; }

private:
    std::optional<resource::ResourceRegistry> registry_;
    std::optional<Sdl3FrameBackend> backend_;
    ResourceId mesh_id_;
    ResourceId material_id_;
};

TEST_F(Sdl3FrameBackendTest, LastCompletedTickIsNulloptBeforeAnySubmit) {
    EXPECT_FALSE(backend().last_completed_tick().has_value());
}

TEST_F(Sdl3FrameBackendTest, SubmitDoesNotThrowAndEventuallyReportsTheTickCompleted) {
    const Frame frame{.tick = core::Time{.ticks = 1}, .draw_commands = {}};
    EXPECT_NO_THROW(backend().submit(frame));

    const std::optional<core::Time> completed = poll_until_completed();

    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(*completed, (core::Time{.ticks = 1}));
}

TEST_F(Sdl3FrameBackendTest, RepeatedSubmitsEventuallyTrackTheLatestCompletedTick) {
    backend().submit(Frame{.tick = core::Time{.ticks = 1}, .draw_commands = {}});
    backend().submit(Frame{.tick = core::Time{.ticks = 2}, .draw_commands = {}});

    const std::optional<core::Time> completed = poll_until_completed();

    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(*completed, (core::Time{.ticks = 2}));
}

TEST_F(Sdl3FrameBackendTest, SubmitDrawsARealResolvedDrawCommandWithoutThrowing) {
    // Issue #154: the actual point of this backend now - a DrawCommand whose
    // mesh/material both resolve, decode, and upload successfully is drawn
    // for real (resolve -> decode -> GPU upload -> bind -> indexed draw),
    // not ignored the way issue #151's own round left it.
    const Frame frame{
        .tick = core::Time{.ticks = 3},
        .draw_commands =
            {
                DrawCommand{
                    .entity = EntityRef{}, .transform = {}, .mesh = mesh_id(), .material = material_id()},
            },
    };

    EXPECT_NO_THROW(backend().submit(frame));

    const std::optional<core::Time> completed = poll_until_completed();
    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(*completed, (core::Time{.ticks = 3}));
}

TEST_F(Sdl3FrameBackendTest, SubmitSkipsADrawCommandWithAnUnresolvedMeshWithoutThrowing) {
    // "skip, never substitute" (build_frame's own documented convention,
    // frame_builder.hpp) - an unresolved mesh reference must not crash or
    // draw anything in its place.
    const Frame frame{
        .tick = core::Time{.ticks = 4},
        .draw_commands =
            {
                DrawCommand{.entity = EntityRef{},
                            .transform = {},
                            .mesh = ResourceId::from_name("meshes/sdl3-frame-backend/does-not-exist"),
                            .material = material_id()},
            },
    };

    EXPECT_NO_THROW(backend().submit(frame));
}

TEST_F(Sdl3FrameBackendTest, SubmitSkipsADrawCommandWithAnUnresolvedTextureWithoutThrowing) {
    const Frame frame{
        .tick = core::Time{.ticks = 5},
        .draw_commands =
            {
                DrawCommand{.entity = EntityRef{},
                            .transform = {},
                            .mesh = mesh_id(),
                            .material = ResourceId::from_name("textures/sdl3-frame-backend/does-not-exist")},
            },
    };

    EXPECT_NO_THROW(backend().submit(frame));
}

TEST_F(Sdl3FrameBackendTest, SubmitSkipsADrawCommandWithNullMeshAndMaterialWithoutThrowing) {
    // Mirrors NullFrameBackend's own equivalent test (frame_backend_test.cpp):
    // a null ResourceId (build_frame's own "no resource set" sentinel, see
    // this library's README) must be skipped too.
    const Frame frame{
        .tick = core::Time{.ticks = 6},
        .draw_commands =
            {
                DrawCommand{.entity = EntityRef{}, .transform = {}, .mesh = {}, .material = {}},
            },
    };

    EXPECT_NO_THROW(backend().submit(frame));
}

TEST_F(Sdl3FrameBackendTest, MoveConstructionTransfersOwnershipAndLeavesSourceHarmlessToDestroy) {
    backend().submit(Frame{.tick = core::Time{.ticks = 5}, .draw_commands = {}});

    Sdl3FrameBackend moved{std::move(backend())};

    std::optional<core::Time> completed;
    for (int attempt = 0; attempt < 1000 && !completed.has_value(); ++attempt) {
        completed = moved.last_completed_tick();
    }
    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(*completed, (core::Time{.ticks = 5}));

    // The moved-from instance's destructor must not double-release SDL /
    // the GPU device it no longer owns - reaching this line without a
    // crash (ASan/UBSan enabled in the debug preset) is the assertion.
}

TEST(Sdl3FrameBackendConstruction, FailureReportsSdlErrorTextInTheException) {
    // Forces a real failure deterministically, rather than relying on this
    // sandbox happening to have no GPU: SDL_HINT_VIDEO_DRIVER set to a
    // driver name that does not exist makes SDL_Init itself fail, before
    // this backend's caches are ever constructed - an empty registry is
    // sufficient here.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "this-video-driver-does-not-exist");

    const resource::ResourceRegistry registry{{}};
    try {
        const Sdl3FrameBackend backend{registry, "atlas-render-tests", 64, 64, SDL_WINDOW_HIDDEN};
        FAIL() << "expected Sdl3FrameBackend construction to throw with an invalid video driver";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_FALSE(message.empty());
    }

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
}

} // namespace
} // namespace atlas::render
