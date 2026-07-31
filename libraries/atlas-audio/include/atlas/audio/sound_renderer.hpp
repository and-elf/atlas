#pragma once

#include "atlas/entity/entity_ref.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/runtime/property_store.hpp"

#include <span>
#include <vector>

namespace atlas::audio {

// One voice's fully composed, mix-ready state (spec §19's State -> Renderer
// -> Output pattern: "game state flows into a renderer, which produces
// output"). `render()` below is the "renderer" step - it never composes
// contributions itself (that stays atlas-runtime's job, §20); it only
// consumes each voice's already-resolved effective gain/pan and clamps them
// into the range this mixer accepts, then reports which cue is playing for
// which entity.
//
// A basic aggregate (rule of zero): no invariant beyond what its four plain
// fields already state - nothing here needs a constructor to protect.
struct ResolvedCue {
    EntityRef source;
    ResourceId cue;
    float effective_gain = 0.0F;
    float effective_pan = 0.0F;

    friend constexpr bool operator==(const ResolvedCue&, const ResolvedCue&) noexcept = default;
};

// Clamps a composed gain value into the linear [0, 1] range this mixer
// accepts. A capability contributing an out-of-range gain (e.g. a buggy
// Multiplicative stack exceeding 1.0) is clamped rather than left to
// silently propagate undefined behavior further down the pipeline - the
// clamp itself is the documented, deterministic policy (mirrors §6's "never
// silently coerce ... explicitly reject" in spirit, applied to composed
// render state rather than a request).
[[nodiscard]] constexpr float clamp_gain(float gain) noexcept {
    if (gain < 0.0F) {
        return 0.0F;
    }
    if (gain > 1.0F) {
        return 1.0F;
    }
    return gain;
}

// Clamps a composed pan value into [-1, 1] (full left .. full right).
[[nodiscard]] constexpr float clamp_pan(float pan) noexcept {
    if (pan < -1.0F) {
        return -1.0F;
    }
    if (pan > 1.0F) {
        return 1.0F;
    }
    return pan;
}

// Renders the current tick's composed audio state into a deterministically
// ordered list of resolved, mix-ready cue entries - the "sound output" this
// library produces this round (see the library README for why raw sample
// synthesis is deferred rather than attempted here).
//
// `voices` fixes the output order explicitly: PropertyStore is backed by
// std::unordered_map, whose iteration order is not part of its contract, so
// this function only ever looks entities up by the caller-supplied order,
// never iterates a store directly (spec §4: avoid unordered iteration
// anywhere it could affect output).
//
// A voice missing any one of its three composed properties this tick (cue,
// gain, or pan) is skipped entirely rather than defaulted - "not fully
// composed yet" is treated as "not currently audible," never guessed at.
[[nodiscard]] std::vector<ResolvedCue> render(std::span<const EntityRef> voices,
                                              const atlas::runtime::PropertyStore<ResourceId>& cues,
                                              const atlas::runtime::PropertyStore<float>& gains,
                                              const atlas::runtime::PropertyStore<float>& pans);

} // namespace atlas::audio
