#include "atlas/entity/entity_ref.hpp"
#include "atlas/input/intent.hpp"
#include "atlas/input/intent_router.hpp"
#include "atlas/input/scripted_raw_signal_source.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace atlas::input {
namespace {

constexpr atlas::EntityRef player{1, 0};

TEST(IntentRouter, EmptyBindingSetProducesNoIntentsForAnyRawSignal) {
    ScriptedRawSignalSource source{{{RawSignalEvent{.signal = RawSignalId{"KeyE"}, .value = 1.0F}}}};
    const IntentRouter router{{}};

    const std::vector<Intent> intents = router.poll(source, player);

    EXPECT_TRUE(intents.empty());
}

TEST(IntentRouter, UnboundRawSignalIsIgnoredRatherThanProducingAnIntent) {
    ScriptedRawSignalSource source{{{RawSignalEvent{.signal = RawSignalId{"KeyQ"}, .value = 1.0F}}}};
    const IntentRouter router{
        {InputBinding{.raw_signal = RawSignalId{"KeyE"}, .intent = IntentId{"Interact"}}}};

    const std::vector<Intent> intents = router.poll(source, player);

    EXPECT_TRUE(intents.empty());
}

TEST(IntentRouter, BoundRawSignalProducesItsIntentCarryingTheSignalsValueAndPollingEntity) {
    ScriptedRawSignalSource source{{{RawSignalEvent{.signal = RawSignalId{"KeyE"}, .value = 1.0F}}}};
    const IntentRouter router{
        {InputBinding{.raw_signal = RawSignalId{"KeyE"}, .intent = IntentId{"Interact"}}}};

    const std::vector<Intent> intents = router.poll(source, player);

    ASSERT_EQ(intents.size(), 1U);
    EXPECT_EQ(intents[0].id, IntentId{"Interact"});
    EXPECT_EQ(intents[0].entity, player);
    EXPECT_EQ(intents[0].axis, 1.0F);
}

TEST(IntentRouter, MultipleRawSignalsInOnePollProduceIntentsInObservedOrder) {
    ScriptedRawSignalSource source{{{
        RawSignalEvent{.signal = RawSignalId{"KeyW"}, .value = 1.0F},
        RawSignalEvent{.signal = RawSignalId{"KeyE"}, .value = 1.0F},
    }}};
    const IntentRouter router{{
        InputBinding{.raw_signal = RawSignalId{"KeyW"}, .intent = IntentId{"MoveForward"}},
        InputBinding{.raw_signal = RawSignalId{"KeyE"}, .intent = IntentId{"Interact"}},
    }};

    const std::vector<Intent> intents = router.poll(source, player);

    ASSERT_EQ(intents.size(), 2U);
    EXPECT_EQ(intents[0].id, IntentId{"MoveForward"});
    EXPECT_EQ(intents[1].id, IntentId{"Interact"});
}

TEST(IntentRouter, MixOfBoundAndUnboundSignalsOnlyEmitsForBoundOnes) {
    ScriptedRawSignalSource source{{{
        RawSignalEvent{.signal = RawSignalId{"KeyE"}, .value = 1.0F},
        RawSignalEvent{.signal = RawSignalId{"KeyQ"}, .value = 1.0F},
    }}};
    const IntentRouter router{
        {InputBinding{.raw_signal = RawSignalId{"KeyE"}, .intent = IntentId{"Interact"}}}};

    const std::vector<Intent> intents = router.poll(source, player);

    ASSERT_EQ(intents.size(), 1U);
    EXPECT_EQ(intents[0].id, IntentId{"Interact"});
}

// The entire point of this library (§5, Input as Intent): a capability-facing
// consumer only ever observes Intent, never a raw signal name/code. `consume`
// below is written the way real capability code would be - its signature
// cannot even name RawSignalId/RawSignalEvent, so this boundary is enforced
// at compile time, not merely asserted at runtime.
std::vector<IntentId> consume(const std::vector<Intent>& intents) {
    std::vector<IntentId> ids;
    ids.reserve(intents.size());
    for (const Intent& intent : intents) {
        ids.push_back(intent.id);
    }
    return ids;
}

TEST(IntentRouter, CapabilityFacingConsumerOnlyEverObservesIntents) {
    ScriptedRawSignalSource source{{{RawSignalEvent{.signal = RawSignalId{"KeyE"}, .value = 1.0F}}}};
    const IntentRouter router{
        {InputBinding{.raw_signal = RawSignalId{"KeyE"}, .intent = IntentId{"Interact"}}}};

    const std::vector<IntentId> ids = consume(router.poll(source, player));

    ASSERT_EQ(ids.size(), 1U);
    EXPECT_EQ(ids[0], IntentId{"Interact"});
}

TEST(IntentRouter, PollingASourceThatHasRunOutOfScriptProducesNoIntents) {
    ScriptedRawSignalSource source{{{RawSignalEvent{.signal = RawSignalId{"KeyE"}, .value = 1.0F}}}};
    const IntentRouter router{
        {InputBinding{.raw_signal = RawSignalId{"KeyE"}, .intent = IntentId{"Interact"}}}};

    (void)router.poll(source, player);
    const std::vector<Intent> intents = router.poll(source, player);

    EXPECT_TRUE(intents.empty());
}

} // namespace
} // namespace atlas::input
