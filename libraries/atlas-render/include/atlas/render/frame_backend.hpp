#pragma once

#include "atlas/core/time.hpp"
#include "atlas/render/frame.hpp"

#include <concepts>
#include <optional>

namespace atlas::render {

// The compile-time contract (spec §5: "checked like a C++ concept, never a
// runtime interface table or virtual dispatch lookup") every render backend
// - real or null - must satisfy. Which concrete type actually gets compiled
// into a given build is a configure-time CMake choice (ATLAS_RENDER_BACKEND,
// see libraries/atlas-render/CMakeLists.txt), never a runtime factory or
// plugin lookup (spec §4).
//
// A conforming backend must always be handed the *complete*, uncensored
// Frame build_frame produced - never a pre-filtered/pre-culled subset.
// Culling is entirely the backend's own concern (issue #117, resolved): a
// real backend is free to cull internally (CPU or GPU - e.g. a compute-
// shader visibility pass feeding an indirect-draw buffer), but nothing
// upstream of submit() ever does it on the backend's behalf. This
// requirement is a documented contract obligation, not something the
// concept below can check structurally - a concept only constrains
// submit()'s signature, never what a caller chooses to pass it.
//
// last_completed_tick() reports the tick of the most recent Frame the
// backend has actually *finished* presenting (e.g. a GPU fence signaled,
// the frame was flipped to screen) - not merely accepted via submit().
// Reuses Frame::tick (core::Time) rather than a separate frame-id type.
// Lets a caller do its own backpressure/batching (skip building a new
// Frame until the backend reports it has caught up) and gives frame-drop
// diagnostics / an FPS meter for free by sampling this over time - Atlas
// only provides the signal, never a batching policy, the same division of
// responsibility issue #117 drew for culling.
template <typename T>
concept FrameBackend = requires(T& backend, const Frame& frame) {
    { backend.submit(frame) } -> std::same_as<void>;
    { backend.last_completed_tick() } -> std::same_as<std::optional<core::Time>>;
};

} // namespace atlas::render
