#pragma once

#include "atlas/entity/entity_ref.hpp"
#include "atlas/render/animation_decode_cache.hpp"
#include "atlas/render/animation_state.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/resource/resource_registry.hpp"
#include "atlas/runtime/property_store.hpp"

#include <span>
#include <string_view>

namespace atlas::render {

// Which skeleton resource, and whether to loop, a CurrentAnimation clip
// should be sampled against - the runtime-side counterpart of #45's
// atlas::rcc::CompiledAnimationMetadata{skeleton, loop, playback_rate}
// (tools/atlas-rcc/include/atlas/rcc/resource_table.hpp), needed because
// that mapping is a build-time-only fact: pack_resource_blob never embeds
// it into the packed blob ResourceRegistry::resolve() reads at runtime
// (see that tool's own README, "animation: metadata never reaches
// pack_resource_blob"), and atlas-render must never depend on atlas-rcc (a
// tool) to read it back out regardless (spec §5, dependency direction) -
// CurrentAnimation::clip alone is not enough information for
// sync_animation_poses (below) to know what to sample against. Whoever
// composes CurrentAnimation for an entity (a capability, or a host's own
// wiring - not yet built, the same "declared vocabulary only" stance
// AnimationPlaybackRate's own doc comment already takes) is expected to set
// this alongside it, the same way Renderable's mesh/material are set
// alongside a Transform. Composed the same way CurrentAnimation itself is
// (spec §20, Priority Override) - not implemented here, since no capability
// contributing to either exists yet. A basic aggregate (rule of zero): no
// invariant beyond ordinary value semantics.
//
// playback_rate is deliberately not carried here - see
// AnimationPlaybackRate's own existing doc comment (animation_state.hpp):
// actually reading/using it to scale elapsed time is explicitly out of
// issue #229's scope, a clean follow-up once it's needed.
struct AnimationPlaybackConfig {
    ResourceId skeleton;
    bool loop = false;
};

// How long the entity has been playing its current clip - presentation-only,
// wall-clock-smoothed bookkeeping (spec §4's own carve-out: "Wall-clock time
// may be used only for presentation-only concerns... must never feed back
// into simulation state"), never simulation state and never replicated. A
// dedicated small type rather than a bare PropertyStore<double> so a reader
// of sync_animation_poses's own signature immediately sees what the value
// means, the same reasoning AnimationPlaybackRate's own doc comment gives
// for wrapping a bare float. A basic aggregate (rule of zero): no invariant
// beyond ordinary value semantics.
struct AnimationClipTime {
    double elapsed_seconds = 0.0;
};

// Presentation-only glue between simulation/composed state and the
// PropertyStore<AnimationPose> build_frame (frame_builder.hpp, issue #46)
// consumes - mirrors demo/presentation_sync.hpp's own sync_transforms
// pattern ("a small glue function between simulation state and a
// PropertyStore build_frame consumes") but lives in atlas-render itself,
// not demo/: resolving a clip resource, sampling it against a skeleton, and
// writing the result into a PropertyStore is mechanism (spec §2), not
// gameplay meaning - demo's own sync_transforms earns its place in demo/
// only because Position -> Transform is itself a demo-authored gameplay
// concept (2D movement), which nothing here is.
//
// For every entity in `entities` that composes a CurrentAnimation:
// - `clip_times` is always advanced by `delta_seconds` first, regardless of
//   whether a pose can be resolved this call - the elapsed-time accumulator
//   keeps ticking even through a tick where the clip/skeleton hasn't
//   resolved yet, the same "keep advancing, don't reset on a transient
//   failure" stance a wall-clock-smoothed accumulator should take;
// - if `playback_configs` has no entry, or the clip fails to resolve/decode
///  (AnimationDecodeCache), or the referenced skeleton fails to resolve
//   (ResourceRegistry) or decode (decode_skeleton), or sample_animation_pose
//   itself returns std::nullopt (mismatched joint count, or no keyframes),
//   this entity's entry in `poses` is left untouched - never cleared, never
//   overwritten with a placeholder - so a previously-resolved pose keeps
//   being consumed by build_frame rather than the entity spuriously
//   disappearing for one bad tick; the very first tick before anything
//   has ever resolved is exactly build_frame's own documented "animated,
//   but no pose resolved yet" skip case;
// - otherwise, `poses` is written with the freshly-sampled AnimationPose.
//
// `skeleton_type_name` is the ResourceRegistry asset-type string skeleton
// resources were registered under (e.g. "Skeleton") - a caller-supplied
// convention, the same reasoning AnimationDecodeCache's own `type_name`
// parameter documents. No caching exists for the skeleton resolve/decode
// step (unlike the clip, which goes through `animation_cache`) - #228
// never added one, and adding one is out of this issue's own scope; a
// clean, obvious follow-up once skeleton re-decoding every call is shown to
// matter in practice.
void sync_animation_poses(std::span<const EntityRef> entities,
                          const runtime::PropertyStore<CurrentAnimation>& current_animations,
                          const runtime::PropertyStore<AnimationPlaybackConfig>& playback_configs,
                          runtime::PropertyStore<AnimationClipTime>& clip_times,
                          runtime::PropertyStore<AnimationPose>& poses,
                          AnimationDecodeCache& animation_cache,
                          const resource::ResourceRegistry& registry,
                          std::string_view skeleton_type_name,
                          double delta_seconds);

} // namespace atlas::render
