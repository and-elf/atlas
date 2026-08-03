#pragma once

#include "atlas/core/time.hpp"
#include "atlas/render/camera.hpp"
#include "atlas/render/frame.hpp"
#include "atlas/render/frame_backend.hpp"
#include "atlas/render/mesh_upload_cache.hpp"
#include "atlas/render/sdl3_distance_cull_pipeline.hpp"
#include "atlas/render/sdl3_mesh_pipeline.hpp"
#include "atlas/render/texture_upload_cache.hpp"
#include "atlas/resource/resource_registry.hpp"
#include "atlas/windowing/sdl3_shared_window.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace atlas::render {

// The first real (non-null) atlas::render::FrameBackend (issue #151, the
// first slice of #69's real GPU/windowing backend): an SDL3 window plus an
// SDL_GPU device, claimed for presentation. Satisfies the same FrameBackend
// concept NullFrameBackend does (frame_backend.hpp) - a caller composing a
// render loop never branches on which concrete backend it was handed.
//
// Scope for this round (issue #154, see this library's README "What's
// implemented"/"Scoping decisions"): submit() resolves each DrawCommand's
// mesh/material through MeshUploadCache/TextureUploadCache, skips any
// DrawCommand whose mesh or texture failed to resolve/decode/upload (never
// substituting a placeholder), builds a model matrix from its Transform
// (transform.hpp's to_model_matrix - applied alone, with no real Camera/
// view-projection concept existing anywhere in Atlas yet, see this
// library's README). This supersedes issue #153's Sdl3TrianglePipeline
// entirely (removed as part of this issue) - real content, not a hardcoded
// triangle, is what submit() draws now.
//
// Issue #156: every surviving DrawCommand is now issued through a real
// GPU-driven distance-culling pass (sdl3_distance_cull_pipeline.hpp) rather
// than an unconditional direct draw - a compute pass tests each one's
// Transform.position against distance_cull (constructor parameter below)
// and writes the result into an indirect-argument buffer, and submit()
// issues one SDL_DrawGPUIndexedPrimitivesIndirect call per surviving
// DrawCommand (draw_count=1, always issued regardless of that DrawCommand's
// own cull outcome - see this library's README "Scoping decisions" for why
// the CPU still never decides visibility itself even though it always
// issues the call). Frame::draw_commands's own order is preserved
// throughout - submit() always receives, and iterates, the complete,
// uncensored Frame (issue #117/#148's contract).
//
// An encapsulated class, not a basic aggregate (unlike this library's other
// value types): it owns real OS/GPU resources (an SDL_Window, an
// SDL_GPUDevice, in-flight SDL_GPUFence handles, plus the mesh pipeline, the
// distance-cull compute pipeline, and two upload caches) with a genuine
// invariant to protect - every acquired handle must be released exactly
// once, in the right order, which is exactly the kind of invariant
// CLAUDE.md's Rule of Zero note carves out an exception for (cf.
// atlas::entity::EntityRegistry).
//
// Construction can fail (no GPU/display hardware, no supported SDL_GPU
// backend on this machine - the common case on CI runners, see the README's
// headless-CI decision) - reported by throwing std::runtime_error rather
// than std::expected, per CLAUDE.md's documented libstdc++/Clang
// <expected> incompatibility. A caller in an environment where that is
// expected (e.g. a test) should construct inside a try/catch and treat
// failure as "skip," not as a defect.
class Sdl3FrameBackend {
public:
    // registry must outlive this backend (the same "registry must outlive
    // this cache" contract atlas::audio::DecodeCache's own header documents,
    // threaded through to the two upload caches this backend owns) - a
    // ResourceRegistry is constructed once at host startup and outlives
    // every backend composed against it in every realistic composition.
    // window_title/width/height describe the window to create.
    // extra_window_flags is passed straight through to SDL_CreateWindow so a
    // caller (e.g. this library's own tests, see sdl3_frame_backend_test.cpp)
    // can opt into SDL_WINDOW_HIDDEN without this type needing its own
    // bespoke "headless" parameter - SDL already has the vocabulary for
    // that. distance_cull configures the GPU-driven distance-culling pass
    // every submit() now runs (issue #156) - see sdl3_distance_cull_pipeline.hpp's
    // own doc comment for what it means; the default (origin, 1000 units) is
    // generous enough that no existing caller authoring content near the
    // origin needs to pass anything here at all.
    //
    // Issue #181: camera is the initial active Camera every submit() pushes
    // its combined view-projection matrix from (to_view_projection_matrix,
    // camera.hpp) - set_camera() below updates it afterwards (a real camera
    // moves every tick; distance_cull's own "caller-supplied at
    // construction, never mutated afterwards" stance would not fit this
    // field the same way). Defaults to Camera{}'s own documented "generic,
    // immediately-usable" default (camera.hpp) - a caller whose window
    // aspect ratio differs from that default's 16:9 should override
    // aspect_ratio explicitly, since this constructor has no way to derive
    // it from width/height itself (the window may be resized after
    // construction, and set_camera() is the one mechanism for keeping it in
    // sync either way).
    //
    // Throws std::runtime_error, with SDL_GetError()'s message included, if
    // SDL video initialization, window creation, GPU device creation,
    // claiming the window for that device, building the mesh pipeline, or
    // building the distance-cull compute pipeline fails.
    explicit Sdl3FrameBackend(const resource::ResourceRegistry& registry,
                              const std::string& window_title = "Atlas",
                              int width = 1280,
                              int height = 720,
                              SDL_WindowFlags extra_window_flags = 0,
                              DistanceCullConfig distance_cull = {},
                              Camera camera = {});

    // Issue #174: the shared-window alternate constructor - claims
    // shared_window's already-created SDL_Window for this backend's own
    // SDL_GPUDevice instead of creating (and later destroying) its own
    // window, so this backend presents into the same single window a real
    // atlas::input::Sdl3RawSignalSource constructed against the same
    // Sdl3SharedWindow reads real OS keyboard/mouse focus from. registry and
    // shared_window must both outlive this instance. Window
    // creation/destruction remain entirely shared_window's responsibility -
    // this constructor only creates/tears down the GPU device and this
    // backend's own pipelines/caches.
    //
    // Throws std::runtime_error, with SDL_GetError()'s message included, if
    // GPU device creation, claiming the window for that device, building
    // the mesh pipeline, or building the distance-cull compute pipeline
    // fails.
    explicit Sdl3FrameBackend(const resource::ResourceRegistry& registry,
                              windowing::Sdl3SharedWindow& shared_window,
                              DistanceCullConfig distance_cull = {},
                              Camera camera = {});

    ~Sdl3FrameBackend();

    // Copying would require duplicating GPU device/window ownership, which
    // SDL_GPU has no defined semantics for - deleted, not implemented.
    Sdl3FrameBackend(const Sdl3FrameBackend&) = delete;
    Sdl3FrameBackend& operator=(const Sdl3FrameBackend&) = delete;

    Sdl3FrameBackend(Sdl3FrameBackend&& other) noexcept;
    Sdl3FrameBackend& operator=(Sdl3FrameBackend&& other) noexcept;

    // Acquires the swapchain texture, clears it, runs the GPU-driven
    // distance-cull pass and draws every surviving DrawCommand in
    // frame.draw_commands via an indirect draw call (see class doc comment
    // above), and presents - against a real SDL_GPUFence tracking when this
    // submission's GPU work actually completes, never NullFrameBackend's
    // "instantly complete" shortcut.
    void submit(const Frame& frame);

    // Issue #181: replaces the active Camera every subsequent submit() draws
    // against - a settable method rather than only a constructor parameter,
    // since a real camera moves every tick (unlike distance_cull, which is
    // fixed reference-point configuration this class never needed to
    // update after construction). Takes effect on the very next submit()
    // call; has no effect on a Frame already submitted.
    void set_camera(const Camera& camera) { camera_ = camera; }

    // The currently active Camera - primarily for tests verifying
    // set_camera() took effect; submit() itself reads camera_ directly.
    [[nodiscard]] const Camera& camera() const { return camera_; }

    // Polls every fence still pending from a prior submit() and advances the
    // reported tick to the most recent one whose GPU work has actually
    // finished - not merely accepted via submit(). Polls on every call
    // (not just from within submit()) so a caller spinning on this signal
    // without submitting new frames still observes completions as they
    // happen; see frame_backend.hpp's doc comment for why this signal
    // matters (backpressure/batching, frame-drop diagnostics).
    [[nodiscard]] std::optional<core::Time> last_completed_tick();

private:
    struct PendingSubmission {
        core::Time tick;
        SDL_GPUFence* fence = nullptr;
    };

    void poll_completed_fences();
    void release_pending_fences_unconditionally();
    void destroy() noexcept;
    // Shared by both constructors - see this method's own doc comment in
    // sdl3_frame_backend.cpp for exactly what it does and does not own.
    void create_device_and_pipelines(SDL_Window* window, const resource::ResourceRegistry& registry);

    SDL_Window* window_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;
    // Issue #154's real mesh pipeline - constructed once, right after the
    // window is claimed for the device, and torn down explicitly by
    // destroy() (see that method) before the device itself is destroyed.
    // See sdl3_mesh_pipeline.hpp for why this is a plain aggregate managed
    // explicitly here rather than its own RAII member.
    Sdl3MeshPipeline mesh_pipeline_;
    // Issue #156's real distance-cull compute pipeline - constructed and
    // torn down alongside mesh_pipeline_ above, for the same reasons
    // (Sdl3MeshPipeline's own doc comment: Sdl3FrameBackend already owns the
    // SDL_GPUDevice this handle depends on and manages its own teardown
    // order manually).
    Sdl3DistanceCullPipeline distance_cull_pipeline_;
    // The reference point/max distance every submit() tests each
    // DrawCommand's Transform.position against - caller-supplied at
    // construction (issue #156), never mutated afterwards.
    DistanceCullConfig distance_cull_config_;
    // Issue #181: the active Camera every submit() pushes its combined
    // view-projection matrix from - caller-supplied at construction,
    // mutable afterwards via set_camera() (unlike distance_cull_config_
    // above, which never changes post-construction).
    Camera camera_;
    // Resource-upload caches (issue #154): std::optional so a moved-from
    // instance (see the move constructor/assignment below) leaves these
    // empty rather than default-constructed - MeshUploadCache/
    // TextureUploadCache have no default constructor (registry/type_name/
    // device are all supplied once, at construction, per their own "must
    // outlive" contracts) the way Sdl3MeshPipeline's plain-aggregate reset
    // does. Torn down explicitly by destroy() (release(), never their own
    // destructors - see each cache's own doc comment) before the device is
    // destroyed, mirroring mesh_pipeline_'s own explicit teardown.
    std::optional<MeshUploadCache> mesh_cache_;
    std::optional<TextureUploadCache> texture_cache_;
    std::vector<PendingSubmission> pending_;
    std::optional<core::Time> last_completed_tick_;
    // True only for an instance whose constructor actually completed (SDL
    // successfully initialized) - false for a default-moved-from instance,
    // so destroy() never calls SDL_Quit() on behalf of an instance that
    // never called SDL_Init() itself (which would unbalance SDL's own
    // internal per-subsystem init refcount). Gates whether destroy() does
    // anything at all, regardless of owns_window_ below.
    bool owns_sdl_ = false;
    // True only for an instance constructed via the self-contained
    // constructor above (it created window_ and called
    // SDL_Init(SDL_INIT_VIDEO) itself) - false for one constructed via the
    // shared-window constructor (issue #174), where window_ is borrowed from
    // a windowing::Sdl3SharedWindow this instance does not own. Gates
    // whether destroy() destroys window_ / calls SDL_Quit() (see destroy()'s
    // own implementation comment).
    bool owns_window_ = false;
};

static_assert(FrameBackend<Sdl3FrameBackend>);

} // namespace atlas::render
