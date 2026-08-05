#pragma once

#include "atlas/audio/audio_backend.hpp"
#include "atlas/audio/null_audio_backend.hpp"
#include "atlas/audio/sound_renderer.hpp"
#include "atlas/entity/entity_ref.hpp"
#include "atlas/input/intent.hpp"
#include "atlas/input/intent_router.hpp"
#include "atlas/input/raw_signal.hpp"
#include "atlas/render/animation_state.hpp"
#include "atlas/render/frame.hpp"
#include "atlas/render/frame_backend.hpp"
#include "atlas/render/frame_builder.hpp"
#include "atlas/render/renderable.hpp"
#include "atlas/render/transform.hpp"
#include "atlas/request/dispatch.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/runtime/property_store.hpp"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "app.hpp"
#include "door/door.hpp"
#include "input_wiring.hpp"
#include "movement/movement.hpp"
#include "player_resources.hpp"
#include "presentation_sync.hpp"

namespace atlas::demo {

// issue #71: the demo App variant that actually wires
// atlas-input/atlas-render/atlas-audio's mechanism into the tick loop.
// Started against the Null/scripted backends only (#194, part 1); part 2
// (#197) swaps in the real SDL3 backends via #174's shared window, behind
// the exact same template.
//
// Templated on the raw input source (Source, a RawSignalSource) and the
// frame backend (FrameBackendT, a FrameBackend) so a test/production caller
// can inject either NullRawSignalSource/NullFrameBackend or
// Sdl3RawSignalSource/Sdl3FrameBackend - mirroring IntentRouter::poll()
// itself being a template method for the same reason (spec §5, Tiny
// Interface Composability: zero virtual-dispatch cost). Both are
// constructor-injected (moved in already-constructed), never default-
// constructed as a member: Sdl3FrameBackend/Sdl3RawSignalSource are not
// default-constructible (they need a ResourceRegistry&/Sdl3SharedWindow&),
// so this class cannot assume its backend type is. Also templated on the
// audio backend (AudioBackendT, an AudioBackend, defaulted to
// NullAudioBackend) so a test can substitute a state-tracking fake to
// observe that door::DoorOpened actually triggers a cue - NullAudioBackend
// itself is a true no-op with nothing to observe, by design (its own doc
// comment). Like Source/FrameBackendT, AudioBackendT is constructor-injected
// (moved in already-constructed) via the 5-argument constructor below;
// the 4-argument overload default-constructs it for a caller whose
// AudioBackendT actually is default-constructible (Null or a test's
// recording fake, every caller before issue #201) - see that constructor's
// own doc comment for why this still compiles for a non-default-
// constructible AudioBackendT such as atlas::audio::Sdl3AudioBackend, which
// needs a DecodeCache& and only ever uses the 5-argument overload.
//
// Scope, matching #194 exactly: exactly one entity (this demo's single
// local "player") is tracked for render/build_frame and audio::render's
// entity spans - there is no live-entity enumeration API anywhere in this
// codebase (atlas-entity's EntityRegistry exposes none), so a real host
// maintaining a dynamic list is a separate concern this mechanism-proving
// issue does not need to solve. The player's Renderable (issue #200) names
// a real, placeholder mesh/texture pair (player_resources.hpp) packed into
// demo/resources/Mesh.blob/Texture.blob - a real FrameBackendT (e.g.
// Sdl3FrameBackend, constructed against a ResourceRegistry that actually
// knows those blobs) resolves and draws it; NullFrameBackend still receives
// the same real, non-empty Frame each tick, it just does nothing with it.
template <input::RawSignalSource Source,
          render::FrameBackend FrameBackendT,
          audio::AudioBackend AudioBackendT = audio::NullAudioBackend>
class PresentationApp : public App {
public:
    // Delegates to the 5-argument constructor below with a default-
    // constructed AudioBackendT (issue #201) - this overload's body is only
    // ever instantiated for an AudioBackendT that actually IS default-
    // constructible, since C++ only instantiates a class template's member
    // function bodies when they're actually called for a given template-
    // argument combination ("implicit instantiation on use"). Every real
    // call site using this overload today (NullAudioBackend, tests'
    // RecordingAudioBackend) is default-constructible; atlas::audio::
    // Sdl3AudioBackend is not, but nothing here ever calls this overload with
    // AudioBackendT = Sdl3AudioBackend, so this body is simply never
    // instantiated for that type and never causes a compile error - the same
    // reasoning that already lets Source/FrameBackendT be non-default-
    // constructible template parameters (Sdl3RawSignalSource/
    // Sdl3FrameBackend aren't default-constructible either).
    PresentationApp(int argc, char** argv, Source source, FrameBackendT frame_backend)
        : PresentationApp(argc, argv, std::move(source), std::move(frame_backend), AudioBackendT{}) {}

    // issue #201: the real constructor, taking an already-constructed
    // AudioBackendT moved in exactly like Source/FrameBackendT already are -
    // needed so a non-default-constructible AudioBackendT (Sdl3AudioBackend,
    // which needs a DecodeCache&) can be composed into this App at all.
    PresentationApp(
        int argc, char** argv, Source source, FrameBackendT frame_backend, AudioBackendT audio_backend)
        : App(argc, argv),
          source_(std::move(source)),
          router_(default_movement_bindings()),
          frame_backend_(std::move(frame_backend)),
          audio_backend_(std::move(audio_backend)) {
        move_dispatcher_.register_handler(movement::on_move);

        player_ = host().create_entity();
        entities_.push_back(player_);
        composition().movement_position_store.set(player_, movement::Position{});
        composition().movement_movement_speed_store.set(player_, movement::MovementSpeed{.base = 0.0F});
        movement::set_base_speed(ctx(), movement_speed_contributions_, player_, kPlayerSpeed);
        renderables_.set(player_,
                         render::Renderable{
                             .mesh = ResourceId::from_name(kPlayerMeshResourceName),
                             .material = ResourceId::from_name(kPlayerTextureResourceName),
                         });

        // door's own DoorOpened is atlas-audio's own doc-commented example of
        // a one-shot TriggeredCue producer (sound_renderer.hpp) - subscribed
        // once here, not per-tick, since Context::subscribe registers a
        // standing handler invoked synchronously by whichever capability's
        // request handler calls ctx.publish<DoorOpened>() this tick.
        ctx().template subscribe<door::DoorOpened>([this](const door::DoorOpened& event) {
            audio_backend_.trigger(audio::TriggeredCue{
                .source = event.door,
                .cue = event.cue,
                .gain = 1.0F,
                .pan = 0.0F,
            });
        });
    }

protected:
    // Polls this tick's raw input, resolves it into a movement direction,
    // and (if nonzero) dispatches a Move request - before advance_tick
    // resolves it, which on_tick (below) is too late for.
    void pre_tick(std::uint64_t /*next_tick*/) override {
        const std::vector<input::Intent> intents = router_.poll(source_, player_);
        const auto [direction_x, direction_y] = resolve_move_direction(intents);
        if (direction_x != 0.0F || direction_y != 0.0F) {
            const RequestResult result = move_dispatcher_.dispatch(ctx(),
                                                                   movement::Move{
                                                                       .target = player_,
                                                                       .direction_x = direction_x,
                                                                       .direction_y = direction_y,
                                                                       .delta_ticks = 1,
                                                                   });
            (void)result; // A rejected Move (e.g. no authority) is an
                          // ordinary outcome this mechanism-proving wiring
                          // does not otherwise act on.
        }
    }

    // Syncs this tick's composed presentation state and submits it to each
    // backend - the "State -> Renderer -> Output" pass (spec §19) for both
    // atlas-render and atlas-audio.
    void on_tick(std::uint64_t tick) override {
        App::on_tick(tick);

        sync_transforms(entities_, composition().movement_position_store, transforms_);
        const render::Frame frame = render::build_frame(
            entities_, transforms_, renderables_, current_animations_, poses_, core::Time{.ticks = tick});
        frame_backend_.submit(frame);

        const std::vector<audio::ResolvedCue> resolved_cues =
            audio::render(entities_, audio_cues_, audio_gains_, audio_pans_);
        audio_backend_.submit(resolved_cues);
    }

    [[nodiscard]] EntityRef player() const noexcept { return player_; }
    [[nodiscard]] FrameBackendT& frame_backend() noexcept { return frame_backend_; }
    [[nodiscard]] AudioBackendT& audio_backend() noexcept { return audio_backend_; }

    // Exposes the exact state on_tick() feeds into build_frame() (issue
    // #200) - lets a test call build_frame directly against this App's real,
    // composed Transform/Renderable state to prove the resolved DrawCommand
    // is correct, without needing a real GPU/FrameBackendT that retains its
    // own draw commands (NullFrameBackend deliberately doesn't).
    [[nodiscard]] std::span<const EntityRef> tracked_entities() const noexcept { return entities_; }
    [[nodiscard]] runtime::PropertyStore<render::Transform>& transforms() noexcept { return transforms_; }
    [[nodiscard]] runtime::PropertyStore<render::Renderable>& renderables() noexcept { return renderables_; }

    // Issue #46: current_animations_/poses_ are empty for now - nothing in
    // this demo populates them yet (a future capability's job, out of
    // scope here). Exposed the same way transforms()/renderables() already
    // are, so a test calling build_frame directly (issue #200's own
    // precedent) can pass them through unchanged.
    [[nodiscard]] runtime::PropertyStore<render::CurrentAnimation>& current_animations() noexcept {
        return current_animations_;
    }
    [[nodiscard]] runtime::PropertyStore<render::AnimationPose>& poses() noexcept { return poses_; }

private:
    // A demo-authored constant (units/second), not a platform default -
    // matches how demo/tests/movement_test.cpp's own scenarios pick an
    // arbitrary base speed for their own proof-of-mechanism purposes.
    static constexpr float kPlayerSpeed = 5.0F;

    Source source_;
    input::IntentRouter router_;
    request::Dispatcher<movement::Move> move_dispatcher_;
    movement::ContributionRegistry movement_speed_contributions_;
    EntityRef player_;
    std::vector<EntityRef> entities_;

    runtime::PropertyStore<render::Transform> transforms_;
    runtime::PropertyStore<render::Renderable> renderables_;
    runtime::PropertyStore<render::CurrentAnimation> current_animations_;
    runtime::PropertyStore<render::AnimationPose> poses_;
    FrameBackendT frame_backend_;

    runtime::PropertyStore<ResourceId> audio_cues_;
    runtime::PropertyStore<float> audio_gains_;
    runtime::PropertyStore<float> audio_pans_;
    AudioBackendT audio_backend_;
};

} // namespace atlas::demo
