#include "atlas/runtime/action.hpp"

#include <gtest/gtest.h>

namespace atlas::runtime {
namespace {

// A minimal Cancellable-shaped type - deliberately not any real capability's
// property, so these tests exercise action.hpp's own contract in isolation,
// the same way property_composition_test.cpp exercises resolve_additive/
// resolve_multiplicative against plain std::int32_t rather than a real
// property type.
struct TestAction {
    ActionState action_state = ActionState::Started;
    bool cancel_requested = false;
};

static_assert(Cancellable<TestAction>);

struct NotCancellable {
    int value = 0;
};

static_assert(!Cancellable<NotCancellable>);

TEST(RequestCancel, SetsTheFlagWithoutTouchingActionStateItself) {
    TestAction action;

    request_cancel(action);

    EXPECT_TRUE(action.cancel_requested);
    // action_state itself is untouched here - only advance_action (below)
    // ever transitions it, and only the next time the action is advanced.
    EXPECT_EQ(action.action_state, ActionState::Started);
}

TEST(AdvanceAction, RunsOnAdvanceWhenNoCancelIsPending) {
    TestAction action{.action_state = ActionState::Ongoing, .cancel_requested = false};
    bool advance_called = false;
    bool cancel_called = false;

    advance_action(
        action, [&](TestAction&) { cancel_called = true; }, [&](TestAction&) { advance_called = true; });

    EXPECT_TRUE(advance_called);
    EXPECT_FALSE(cancel_called);
    EXPECT_EQ(action.action_state, ActionState::Ongoing);
}

TEST(AdvanceAction, RunsOnCancelInsteadOfOnAdvanceWhenCancelIsPending) {
    // The core guarantee: cancellation is checked and handled *before* any
    // normal advance logic runs, and on_advance is not invoked at all this
    // call - not "cancel wins if both would apply," but "cancel is checked
    // first, full stop."
    TestAction action{.action_state = ActionState::Ongoing, .cancel_requested = true};
    bool advance_called = false;
    bool cancel_called = false;

    advance_action(
        action, [&](TestAction&) { cancel_called = true; }, [&](TestAction&) { advance_called = true; });

    EXPECT_TRUE(cancel_called);
    EXPECT_FALSE(advance_called);
}

TEST(AdvanceAction, TransitionsToCancelledAndClearsTheFlag) {
    TestAction action{.action_state = ActionState::Ongoing, .cancel_requested = true};

    advance_action(action, [](TestAction&) {}, [](TestAction&) {});

    EXPECT_EQ(action.action_state, ActionState::Cancelled);
    EXPECT_FALSE(action.cancel_requested);
}

TEST(AdvanceAction, IsANoOpWhenAlreadyCancelled) {
    TestAction action{.action_state = ActionState::Cancelled, .cancel_requested = false};
    bool advance_called = false;
    bool cancel_called = false;

    advance_action(
        action, [&](TestAction&) { cancel_called = true; }, [&](TestAction&) { advance_called = true; });

    EXPECT_FALSE(advance_called);
    EXPECT_FALSE(cancel_called);
}

TEST(AdvanceAction, IsANoOpWhenAlreadyCompleted) {
    TestAction action{.action_state = ActionState::Completed, .cancel_requested = false};
    bool advance_called = false;
    bool cancel_called = false;

    advance_action(
        action, [&](TestAction&) { cancel_called = true; }, [&](TestAction&) { advance_called = true; });

    EXPECT_FALSE(advance_called);
    EXPECT_FALSE(cancel_called);
}

TEST(AdvanceAction, IsANoOpWhenAlreadyCompletedEvenIfCancelIsSomehowPending) {
    // Cancel_requested lingering on an already-terminal action (e.g. a
    // cancel event arriving the same tick an action completed, processed
    // after completion) must not retroactively "cancel" something that
    // already finished - once terminal, an action stays terminal until
    // whoever owns it explicitly restarts it (e.g. a fresh BeginCast).
    TestAction action{.action_state = ActionState::Completed, .cancel_requested = true};

    advance_action(action, [](TestAction&) {}, [](TestAction&) {});

    EXPECT_EQ(action.action_state, ActionState::Completed);
    EXPECT_TRUE(action.cancel_requested);
}

TEST(AdvanceAction, OnAdvanceCanTransitionToCompleted) {
    // advance_action itself has no opinion on when an action completes -
    // that is entirely on_advance's own decision, the same way
    // resolve_additive/resolve_multiplicative have no opinion on what a
    // property represents.
    TestAction action{.action_state = ActionState::Ongoing, .cancel_requested = false};

    advance_action(
        action, [](TestAction&) {}, [](TestAction& a) { a.action_state = ActionState::Completed; });

    EXPECT_EQ(action.action_state, ActionState::Completed);
}

} // namespace
} // namespace atlas::runtime
