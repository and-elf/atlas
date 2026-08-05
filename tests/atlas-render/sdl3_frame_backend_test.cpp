#include "atlas/render/frame_backend.hpp"
#include "atlas/render/sdl3_frame_backend.hpp"
#include "atlas/windowing/sdl3_shared_window.hpp"

#include <SDL3/SDL.h>
#include <chrono>
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

// A machine with no real GPU/display hardware and no Vulkan/Metal/D3D12 ICD
// at all is exactly why issue #148 built NullFrameBackend in the first place
// (see this library's README, "headless-CI decision"). Every fixture-based
// test below attempts *real* SDL3 window + SDL_GPU device creation in
// SetUp() and, on failure, calls GTEST_SKIP() with the thrown exception's
// message rather than treating it as a test failure - the constructor
// throwing std::runtime_error is itself the behavior under test on a
// machine like that. SDL_HINT_VIDEO_DRIVER is forced to "offscreen" (issue
// #155) first so windowing itself succeeds headlessly (no DISPLAY needed) -
// NOT "dummy", which issue #151/#153/#154 originally used: reading SDL3's
// own source (src/gpu/vulkan/SDL_vulkan.c, VULKAN_PrepareDriver) shows the
// "dummy" video driver has no Vulkan_CreateSurface implementation at all, so
// SDL_GPU's Vulkan backend unconditionally bails out under it regardless of
// whether a working Vulkan ICD exists - these tests were structurally
// guaranteed to skip under "dummy" even on a machine with a real/software
// GPU. "offscreen" (SDL3's own offscreen video backend, src/video/offscreen/)
// does implement Vulkan_CreateSurface, so with a real ICD present (this
// sandbox has mesa-vulkan-drivers' lavapipe/llvmpipe software rasterizer -
// verified directly, not assumed) GPU device creation now genuinely
// succeeds, giving line coverage over the real success path rather than
// only ever exercising the throwing failure path.
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
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");

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

    // Polls last_completed_tick() until either `timeout` elapses (real wall-
    // clock time, not an iteration count) or the value stops advancing
    // within the final poll_window slice of that budget - never assuming
    // instant completion (that is the entire point of a real GPU fence over
    // NullFrameBackend's "instantly complete" shortcut), and never spinning
    // forever if a driver somehow never signals.
    //
    // Issue #155: this used to be a fixed 1000-iteration busy-loop that (a)
    // returned the moment *any* value appeared at all - a real race with
    // more than one pending submission
    // (RepeatedSubmitsEventuallyTrackTheLatestCompletedTick, below): two
    // fences submitted back-to-back against a real (especially a slower,
    // software-rasterized) GPU can complete far enough apart in wall-clock
    // time that the first fence's completion is observed and returned
    // before the second fence has signaled at all - and (b), even once
    // fixed to spend the entire iteration budget rather than exiting early,
    // still occasionally timed out even for a *single* submission
    // (SubmitDrawsARealResolvedDrawCommandWithoutThrowing, below, which
    // does real resolve/decode/GPU-upload/draw work, not just an empty
    // Frame): 1000 busy-poll iterations is not a stable proxy for wall-clock
    // time - how long that many iterations actually take varies with system
    // load, and a real GPU's completion latency does not scale with how
    // many times this process happened to ask about it. A real
    // std::chrono-based deadline is the only bound that is actually about
    // the thing this helper is waiting for. Nothing this test fixture owns
    // can ask Sdl3FrameBackend "is any fence still pending" directly, so
    // "stopped changing for a little while, this late in a generous
    // deadline" is the closest available proxy for "nothing left to
    // complete." This whole class of race was unreachable as long as every
    // fixture-based test always skipped under the "dummy" video driver (see
    // this class's own doc comment above), so it was never actually
    // observable before now.
    std::optional<core::Time> poll_until_completed(std::chrono::milliseconds timeout = std::chrono::seconds{
                                                       5}) {
        constexpr std::chrono::milliseconds poll_window{50};
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        std::optional<core::Time> completed;
        std::optional<core::Time> last_change_observed_at_value;
        auto last_change_at = std::chrono::steady_clock::now();

        while (std::chrono::steady_clock::now() < deadline) {
            completed = backend().last_completed_tick();
            if (completed != last_change_observed_at_value) {
                last_change_observed_at_value = completed;
                last_change_at = std::chrono::steady_clock::now();
            } else if (completed.has_value() &&
                       std::chrono::steady_clock::now() - last_change_at > poll_window) {
                break;
            }
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
                DrawCommand{.entity = EntityRef{},
                            .transform = {},
                            .mesh = mesh_id(),
                            .material = material_id(),
                            .pose = std::nullopt},
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
                            .material = material_id(),
                            .pose = std::nullopt},
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
                            .material = ResourceId::from_name("textures/sdl3-frame-backend/does-not-exist"),
                            .pose = std::nullopt},
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
                DrawCommand{
                    .entity = EntityRef{}, .transform = {}, .mesh = {}, .material = {}, .pose = std::nullopt},
            },
    };

    EXPECT_NO_THROW(backend().submit(frame));
}

TEST_F(Sdl3FrameBackendTest, MoveConstructionTransfersOwnershipAndLeavesSourceHarmlessToDestroy) {
    backend().submit(Frame{.tick = core::Time{.ticks = 5}, .draw_commands = {}});

    Sdl3FrameBackend moved{std::move(backend())};

    // Polls the *moved-to* instance directly (poll_until_completed() itself
    // only ever polls backend(), the fixture's own instance) - the same
    // real-wall-clock-deadline reasoning poll_until_completed's own doc
    // comment gives applies here too, not just a fixed iteration count.
    constexpr std::chrono::milliseconds timeout{5000};
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::optional<core::Time> completed;
    while (!completed.has_value() && std::chrono::steady_clock::now() < deadline) {
        completed = moved.last_completed_tick();
    }
    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(*completed, (core::Time{.ticks = 5}));

    // The moved-from instance's destructor must not double-release SDL /
    // the GPU device it no longer owns - reaching this line without a
    // crash (ASan/UBSan enabled in the debug preset) is the assertion.
}

// Submits `frame` through `backend` and polls last_completed_tick() until it
// stops advancing (the same real-wall-clock-deadline reasoning
// Sdl3FrameBackendTest::poll_until_completed's own doc comment gives above
// applies here too) - pulled out of the two TEST bodies below purely to keep
// each within this project's clang-tidy cognitive-complexity gate, mirroring
// sdl3_spinning_box_test.cpp's own submit_one_tick, pulled out for the
// identical reason: GTest's EXPECT_NO_THROW/ASSERT macros each expand into
// several nested control-flow layers the checker counts against whichever
// function directly contains them.
std::optional<core::Time> submit_and_poll_for_completion(Sdl3FrameBackend& backend, const Frame& frame) {
    EXPECT_NO_THROW(backend.submit(frame));

    constexpr std::chrono::milliseconds timeout{5000};
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::optional<core::Time> completed;
    while (!completed.has_value() && std::chrono::steady_clock::now() < deadline) {
        completed = backend.last_completed_tick();
    }
    return completed;
}

// Issue #156: mechanism-level integration coverage for the real, public
// Sdl3FrameBackend::submit() entry point - the distance-culling pipeline
// itself is already proven at the pixel level directly
// (sdl3_pixel_correctness_test.cpp) and at the raw indirect-buffer-contents
// level (sdl3_distance_cull_test.cpp); this file's own job is only to prove
// submit() itself, wired up with a real DistanceCullConfig, still behaves -
// doesn't throw, still reports completion - now that it runs a real compute
// pass and issues indirect (not direct) draw calls internally. Cannot
// verify pixels here (submit() only ever targets the window's own
// swapchain, which - like every other fixture in this file - has no real
// presentable surface under "offscreen"; see this file's own top-of-file
// doc comment).
TEST(Sdl3FrameBackendDistanceCull, SubmitWithAFarAwayDrawCommandDoesNotThrowAndStillReportsCompletion) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");

    const ResourceId mesh_id = ResourceId::from_name("meshes/sdl3-frame-backend-cull/triangle");
    const ResourceId material_id = ResourceId::from_name("textures/sdl3-frame-backend-cull/checker");
    const auto mesh_blob_path = write_temp_blob(
        "sdl3_frame_backend_cull_mesh.blob", pack_single_entry_blob(mesh_id, read_fixture("triangle.mesh")));
    const auto texture_blob_path =
        write_temp_blob("sdl3_frame_backend_cull_texture.blob",
                        pack_single_entry_blob(material_id, read_fixture("checker.tex")));
    const resource::ResourceRegistry registry{{
        {"Mesh", mesh_blob_path},
        {"Texture", texture_blob_path},
    }};

    // A tight max_distance around the origin - small enough that the
    // DrawCommand authored far outside it below is genuinely culled by the
    // real compute pass, not merely "still drawn unconditionally" the way
    // issue #154 left things before this issue.
    std::optional<Sdl3FrameBackend> backend;
    try {
        backend.emplace(registry,
                        "atlas-render-tests",
                        64,
                        64,
                        SDL_WINDOW_HIDDEN,
                        DistanceCullConfig{.reference_point = {}, .max_distance = 1.0F});
    } catch (const std::runtime_error& error) {
        GTEST_SKIP() << "No real SDL_GPU-capable backend available in this environment "
                        "(expected on most headless CI runners - see "
                        "libraries/atlas-render/README.md's headless-CI decision): "
                     << error.what();
    }

    const Frame frame{
        .tick = core::Time{.ticks = 42},
        .draw_commands =
            {
                DrawCommand{.entity = EntityRef{},
                            .transform = Transform{.position = {1000.0F, 0.0F, 0.0F},
                                                   .rotation = {},
                                                   .scale = {1.0F, 1.0F, 1.0F}},
                            .mesh = mesh_id,
                            .material = material_id,
                            .pose = std::nullopt},
            },
    };

    // A culled DrawCommand still issues a real (zero-instance) indirect draw
    // call, and the frame still completes normally.
    const std::optional<core::Time> completed = submit_and_poll_for_completion(*backend, frame);
    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(*completed, (core::Time{.ticks = 42}));
}

TEST(Sdl3FrameBackendDistanceCull,
     SubmitWithMixedNearAndFarDrawCommandsDoesNotThrowAndStillReportsCompletion) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");

    const ResourceId mesh_id = ResourceId::from_name("meshes/sdl3-frame-backend-cull/triangle-mixed");
    const ResourceId material_id = ResourceId::from_name("textures/sdl3-frame-backend-cull/checker-mixed");
    const auto mesh_blob_path =
        write_temp_blob("sdl3_frame_backend_cull_mixed_mesh.blob",
                        pack_single_entry_blob(mesh_id, read_fixture("triangle.mesh")));
    const auto texture_blob_path =
        write_temp_blob("sdl3_frame_backend_cull_mixed_texture.blob",
                        pack_single_entry_blob(material_id, read_fixture("checker.tex")));
    const resource::ResourceRegistry registry{{
        {"Mesh", mesh_blob_path},
        {"Texture", texture_blob_path},
    }};

    std::optional<Sdl3FrameBackend> backend;
    try {
        backend.emplace(registry,
                        "atlas-render-tests",
                        64,
                        64,
                        SDL_WINDOW_HIDDEN,
                        DistanceCullConfig{.reference_point = {}, .max_distance = 1.0F});
    } catch (const std::runtime_error& error) {
        GTEST_SKIP() << "No real SDL_GPU-capable backend available in this environment "
                        "(expected on most headless CI runners - see "
                        "libraries/atlas-render/README.md's headless-CI decision): "
                     << error.what();
    }

    // Three DrawCommands in one Frame, deliberately interleaved
    // near/far/near - exercises that the per-entry cull outcome (written by
    // one shared compute dispatch) stays correctly aligned with each
    // entry's own subsequent indirect draw call regardless of position in
    // Frame::draw_commands, not just a single-entry Frame.
    const Frame frame{
        .tick = core::Time{.ticks = 7},
        .draw_commands =
            {
                DrawCommand{.entity = EntityRef{},
                            .transform = Transform{.position = {0.0F, 0.0F, 0.0F},
                                                   .rotation = {},
                                                   .scale = {1.0F, 1.0F, 1.0F}},
                            .mesh = mesh_id,
                            .material = material_id,
                            .pose = std::nullopt},
                DrawCommand{.entity = EntityRef{},
                            .transform = Transform{.position = {1000.0F, 0.0F, 0.0F},
                                                   .rotation = {},
                                                   .scale = {1.0F, 1.0F, 1.0F}},
                            .mesh = mesh_id,
                            .material = material_id,
                            .pose = std::nullopt},
                DrawCommand{.entity = EntityRef{},
                            .transform = Transform{.position = {0.5F, 0.0F, 0.0F},
                                                   .rotation = {},
                                                   .scale = {1.0F, 1.0F, 1.0F}},
                            .mesh = mesh_id,
                            .material = material_id,
                            .pose = std::nullopt},
            },
    };

    const std::optional<core::Time> completed = submit_and_poll_for_completion(*backend, frame);
    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(*completed, (core::Time{.ticks = 7}));
}

// Issue #174: the shared-window alternate constructor - proves
// Sdl3FrameBackend can claim an already-created windowing::Sdl3SharedWindow
// for its own GPU device (rather than creating its own window) and still
// behaves exactly like the self-contained-constructor fixture above (submit
// doesn't throw, completion is eventually reported), and that destroying it
// leaves the still-alive Sdl3SharedWindow's own handle() untouched.
TEST(Sdl3FrameBackendSharedWindow, SubmitDoesNotThrowAndEventuallyReportsCompletionAgainstASharedWindow) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");

    windowing::Sdl3SharedWindow shared_window{"atlas-render-tests-shared", 64, 64, SDL_WINDOW_HIDDEN};
    SDL_Window* const original_handle = shared_window.handle();

    const resource::ResourceRegistry registry{{}};
    std::optional<Sdl3FrameBackend> backend;
    try {
        backend.emplace(registry, shared_window);
    } catch (const std::runtime_error& error) {
        GTEST_SKIP() << "No real SDL_GPU-capable backend available in this environment "
                        "(expected on most headless CI runners - see "
                        "libraries/atlas-render/README.md's headless-CI decision): "
                     << error.what();
    }

    const std::optional<core::Time> completed =
        submit_and_poll_for_completion(*backend, Frame{.tick = core::Time{.ticks = 1}, .draw_commands = {}});
    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(*completed, (core::Time{.ticks = 1}));

    // Destroying the backend must not touch the shared window it borrowed -
    // reaching this line without a crash (ASan/UBSan enabled in the debug
    // preset) and the handle staying identical is the assertion.
    backend.reset();
    EXPECT_EQ(shared_window.handle(), original_handle);
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

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
}

} // namespace
} // namespace atlas::render
