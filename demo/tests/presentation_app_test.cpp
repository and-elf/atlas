#include "atlas/input/scripted_raw_signal_source.hpp"
#include "atlas/render/null_frame_backend.hpp"
#include "atlas/request/dispatch.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

#include "door/door.hpp"
#include "presentation_app.hpp"

namespace atlas::demo {
namespace {

// A state-tracking AudioBackend fake (issue #71 part 1): unlike
// atlas::audio::NullAudioBackend (a true no-op with nothing to observe, by
// its own doc comment's design), this records every submit()/trigger()
// call so a test can assert door::DoorOpened actually reaches
// AudioBackend::trigger() with the expected TriggeredCue.
struct RecordingAudioBackend {
    std::vector<audio::TriggeredCue> triggered;
    std::vector<std::vector<audio::ResolvedCue>> submitted;

    void submit(std::span<const audio::ResolvedCue> cues) {
        submitted.emplace_back(cues.begin(), cues.end());
    }

    void trigger(const audio::TriggeredCue& cue) { triggered.push_back(cue); }
};

static_assert(audio::AudioBackend<RecordingAudioBackend>);

// Exposes PresentationApp's protected surface for these tests, the same
// pattern demo/tests/app_test.cpp's own TestApp already establishes for App
// itself.
template <input::RawSignalSource Source,
          render::FrameBackend FrameBackendT = render::NullFrameBackend,
          audio::AudioBackend AudioBackendT = audio::NullAudioBackend>
class TestPresentationApp : public PresentationApp<Source, FrameBackendT, AudioBackendT> {
public:
    using PresentationApp<Source, FrameBackendT, AudioBackendT>::PresentationApp;

    [[nodiscard]] Context& ctx_for_test() { return this->ctx(); }
    [[nodiscard]] EntityRef player_for_test() const { return this->player(); }
    [[nodiscard]] FrameBackendT& frame_backend_for_test() { return this->frame_backend(); }
    [[nodiscard]] AudioBackendT& audio_backend_for_test() { return this->audio_backend(); }
};

// A real argv-shaped array (argv[argc] must be a null pointer, the same
// contract main()'s own argv has) - mirrors app_test.cpp's own Argv, kept as
// a separate small copy here rather than shared test scaffolding for one
// call site.
class Argv {
public:
    explicit Argv(std::initializer_list<const char*> args) : args_(args.begin(), args.end()) {
        pointers_.reserve(args_.size() + 1);
        for (auto& arg : args_) {
            pointers_.push_back(arg.data());
        }
        pointers_.push_back(nullptr);
    }

    [[nodiscard]] int argc() const { return static_cast<int>(args_.size()); }
    [[nodiscard]] char** argv() { return pointers_.data(); }

private:
    std::vector<std::string> args_;
    std::vector<char*> pointers_;
};

std::vector<std::vector<input::RawSignalEvent>> held_key_frames(std::string_view key_name,
                                                                std::size_t frame_count) {
    return std::vector<std::vector<input::RawSignalEvent>>(
        frame_count, {input::RawSignalEvent{.signal = input::RawSignalId{key_name}, .value = 1.0F}});
}

TEST(PresentationApp, ConstructionSeedsThePlayerWithPositionAndMovementSpeed) {
    Argv args{"demo-host"};
    TestPresentationApp<input::ScriptedRawSignalSource> app(
        args.argc(), args.argv(), input::ScriptedRawSignalSource{{}}, render::NullFrameBackend{});

    const auto position = app.ctx_for_test().get<movement::Position>(app.player_for_test());
    ASSERT_TRUE(position.has_value());
    EXPECT_FLOAT_EQ(position->get().x, 0.0F);
    EXPECT_FLOAT_EQ(position->get().y, 0.0F);

    const auto speed = app.ctx_for_test().get<movement::MovementSpeed>(app.player_for_test());
    ASSERT_TRUE(speed.has_value());
    EXPECT_FLOAT_EQ(speed->get().base, 5.0F);
}

TEST(PresentationApp, HeldForwardIntentMovesThePlayerForwardOverOneSecond) {
    // 60 ticks at the default 60 ticks_per_second is exactly 1 simulated
    // second; at the seeded 5.0 units/second base speed that covers 5.0
    // units - proving pre_tick's poll -> resolve_move_direction -> Move
    // dispatch chain runs once per tick, not just once total.
    Argv args{"demo-host", "--ticks", "60"};
    TestPresentationApp<input::ScriptedRawSignalSource> app(
        args.argc(),
        args.argv(),
        input::ScriptedRawSignalSource{held_key_frames("KeyW", 60)},
        render::NullFrameBackend{});

    EXPECT_EQ(app.run(), 0);

    const auto position = app.ctx_for_test().get<movement::Position>(app.player_for_test());
    ASSERT_TRUE(position.has_value());
    EXPECT_NEAR(position->get().x, 0.0F, 1e-4F);
    EXPECT_NEAR(position->get().y, 5.0F, 1e-4F);
}

TEST(PresentationApp, NoInputLeavesThePlayerPositionUnchanged) {
    Argv args{"demo-host", "--ticks", "10"};
    TestPresentationApp<input::ScriptedRawSignalSource> app(
        args.argc(), args.argv(), input::ScriptedRawSignalSource{{}}, render::NullFrameBackend{});

    EXPECT_EQ(app.run(), 0);

    const auto position = app.ctx_for_test().get<movement::Position>(app.player_for_test());
    ASSERT_TRUE(position.has_value());
    EXPECT_FLOAT_EQ(position->get().x, 0.0F);
    EXPECT_FLOAT_EQ(position->get().y, 0.0F);
}

TEST(PresentationApp, TheNullFrameBackendReceivesAFrameEveryTick) {
    Argv args{"demo-host", "--ticks", "3"};
    TestPresentationApp<input::ScriptedRawSignalSource> app(
        args.argc(), args.argv(), input::ScriptedRawSignalSource{{}}, render::NullFrameBackend{});

    EXPECT_EQ(app.run(), 0);

    ASSERT_TRUE(app.frame_backend_for_test().last_completed_tick().has_value());
    EXPECT_EQ(app.frame_backend_for_test().last_completed_tick()->ticks, 3U);
}

TEST(PresentationApp, DoorOpenedTriggersACueOnTheAudioBackend) {
    Argv args{"demo-host"};
    TestPresentationApp<input::ScriptedRawSignalSource, render::NullFrameBackend, RecordingAudioBackend> app(
        args.argc(), args.argv(), input::ScriptedRawSignalSource{{}}, render::NullFrameBackend{});

    const EntityRef door_entity = app.ctx_for_test().host().create_entity();
    const auto cue = ResourceId::from_name("sfx/door/open");
    app.ctx_for_test().set<door::Door>(door_entity, door::Door{.open = false, .cue = cue});

    request::Dispatcher<door::OpenDoor> dispatcher;
    dispatcher.register_handler(door::on_open_door);
    const RequestResult result = dispatcher.dispatch(app.ctx_for_test(), door::OpenDoor{.door = door_entity});
    ASSERT_TRUE(result.accepted);

    ASSERT_EQ(app.audio_backend_for_test().triggered.size(), 1U);
    EXPECT_EQ(app.audio_backend_for_test().triggered[0].source, door_entity);
    EXPECT_EQ(app.audio_backend_for_test().triggered[0].cue, cue);
}

} // namespace
} // namespace atlas::demo
