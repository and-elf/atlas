#pragma once

#include "atlas/core/time.hpp"
#include "atlas/render/frame.hpp"
#include "atlas/render/frame_backend.hpp"

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
// Scope for this round (see this library's README, "Scoping decisions"):
// submit() clears the swapchain texture to a fixed color and presents it -
// Frame::draw_commands is entirely ignored (real geometry/shaders are
// #153/#154's job, not this one). Only Frame::tick matters here.
//
// An encapsulated class, not a basic aggregate (unlike this library's other
// value types): it owns real OS/GPU resources (an SDL_Window, an
// SDL_GPUDevice, in-flight SDL_GPUFence handles) with a genuine invariant to
// protect - every acquired handle must be released exactly once, in the
// right order, which is exactly the kind of invariant CLAUDE.md's Rule of
// Zero note carves out an exception for (cf. atlas::entity::EntityRegistry).
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
    // window_title/width/height describe the window to create. extra_window_flags
    // is passed straight through to SDL_CreateWindow so a caller (e.g. this
    // library's own tests, see sdl3_frame_backend_test.cpp) can opt into
    // SDL_WINDOW_HIDDEN without this type needing its own bespoke "headless"
    // parameter - SDL already has the vocabulary for that.
    //
    // Throws std::runtime_error, with SDL_GetError()'s message included, if
    // SDL video initialization, window creation, GPU device creation, or
    // claiming the window for that device fails.
    explicit Sdl3FrameBackend(const std::string& window_title = "Atlas",
                              int width = 1280,
                              int height = 720,
                              SDL_WindowFlags extra_window_flags = 0);

    ~Sdl3FrameBackend();

    // Copying would require duplicating GPU device/window ownership, which
    // SDL_GPU has no defined semantics for - deleted, not implemented.
    Sdl3FrameBackend(const Sdl3FrameBackend&) = delete;
    Sdl3FrameBackend& operator=(const Sdl3FrameBackend&) = delete;

    Sdl3FrameBackend(Sdl3FrameBackend&& other) noexcept;
    Sdl3FrameBackend& operator=(Sdl3FrameBackend&& other) noexcept;

    // Acquires the swapchain texture, clears it to a fixed color, and
    // presents it. Frame::draw_commands is deliberately ignored this round
    // (see class doc comment above); only frame.tick is recorded, against
    // a real SDL_GPUFence tracking when this submission's GPU work actually
    // completes - never NullFrameBackend's "instantly complete" shortcut.
    void submit(const Frame& frame);

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

    SDL_Window* window_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;
    std::vector<PendingSubmission> pending_;
    std::optional<core::Time> last_completed_tick_;
    // True only for an instance whose constructor actually completed (SDL
    // successfully initialized) - false for a default-moved-from instance,
    // so destroy() never calls SDL_Quit() on behalf of an instance that
    // never called SDL_Init() itself (which would unbalance SDL's own
    // internal per-subsystem init refcount).
    bool owns_sdl_ = false;
};

static_assert(FrameBackend<Sdl3FrameBackend>);

} // namespace atlas::render
