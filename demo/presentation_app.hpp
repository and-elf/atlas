#pragma once

#include "atlas/audio/audio_backend.hpp"
#include "atlas/audio/null_audio_backend.hpp"
#include "atlas/audio/sound_renderer.hpp"
#include "atlas/entity/entity_ref.hpp"
#include "atlas/input/intent.hpp"
#include "atlas/input/intent_router.hpp"
#include "atlas/input/raw_signal.hpp"
#include "atlas/render/frame.hpp"
#include "atlas/render/frame_builder.hpp"
#include "atlas/render/null_frame_backend.hpp"
#include "atlas/render/renderable.hpp"
#include "atlas/render/transform.hpp"
#include "atlas/request/dispatch.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/runtime/property_store.hpp"

#include <cstdint>
#include <utility>
#include <vector>

#include "app.hpp"
#include "door/door.hpp"
#include "input_wiring.hpp"
#include "movement/movement.hpp"
#include "presentation_sync.hpp"

namespace atlas::demo {

// issue #71 part 1: the demo App variant that actually wires
// atlas-input/atlas-render/atlas-audio's mechanism into the tick loop -
// against the Null/scripted backends only (see issue #194's scope note:
// real SDL3 backends, via #174's shared window, are a separate follow-up).
//
// Templated on the raw input source (Source, a RawSignalSource) so a test
// can inject ScriptedRawSignalSource while demo-host's own main.cpp uses
// NullRawSignalSource - mirroring IntentRouter::poll() itself being a
// template method for the same reason (spec §5, Tiny Interface
// Composability: zero virtual-dispatch cost). Also templated on the audio
// backend (AudioBackendT, an AudioBackend, defaulted to NullAudioBackend)
// purely so a test can substitute a state-tracking fake to observe that
// door::DoorOpened actually triggers a cue - NullAudioBackend itself is a
// true no-op with nothing to observe, by design (its own doc comment).
// NullFrameBackend needs no equivalent seam: unlike NullAudioBackend, it
// already tracks the last tick it was submitted (last_completed_tick()),
// so it alone is enough to test the render half of this wiring.
//
// Scope, matching #194 exactly: exactly one entity (this demo's single
// local "player") is tracked for render/build_frame and audio::render's
// entity spans - there is no live-entity enumeration API anywhere in this
// codebase (atlas-entity's EntityRegistry exposes none), so a real host
// maintaining a dynamic list is a separate concern this mechanism-proving
// issue does not need to solve. Renderable stays populated with nothing (no
// mesh/material assets exist yet), so build_frame legitimately produces an
// empty Frame every tick - proving the call site exists and runs, not
// fabricating placeholder asset data.
template <input::RawSignalSource Source, audio::AudioBackend AudioBackendT = audio::NullAudioBackend>
class PresentationApp : public App {
public:
    PresentationApp(int argc, char** argv, Source source)
        : App(argc, argv), source_(std::move(source)), router_(default_movement_bindings()) {
        move_dispatcher_.register_handler(movement::on_move);

        player_ = host().create_entity();
        entities_.push_back(player_);
        composition().movement_position_store.set(player_, movement::Position{});
        composition().movement_movement_speed_store.set(player_, movement::MovementSpeed{.base = 0.0F});
        movement::set_base_speed(ctx(), movement_speed_contributions_, player_, kPlayerSpeed);

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
        const std::vector<input::Intent> intents = router_.poll(source_);
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
        const render::Frame frame =
            render::build_frame(entities_, transforms_, renderables_, core::Time{.ticks = tick});
        frame_backend_.submit(frame);

        const std::vector<audio::ResolvedCue> resolved_cues =
            audio::render(entities_, audio_cues_, audio_gains_, audio_pans_);
        audio_backend_.submit(resolved_cues);
    }

    [[nodiscard]] EntityRef player() const noexcept { return player_; }
    [[nodiscard]] render::NullFrameBackend& frame_backend() noexcept { return frame_backend_; }
    [[nodiscard]] AudioBackendT& audio_backend() noexcept { return audio_backend_; }

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
    render::NullFrameBackend frame_backend_;

    runtime::PropertyStore<ResourceId> audio_cues_;
    runtime::PropertyStore<float> audio_gains_;
    runtime::PropertyStore<float> audio_pans_;
    AudioBackendT audio_backend_;
};

} // namespace atlas::demo
