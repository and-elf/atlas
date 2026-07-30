#pragma once

#include <concepts>
#include <cstdint>
#include <utility>

namespace atlas::runtime {

// The lifecycle every request that doesn't resolve within its own call
// eventually passes through - a long-running server-side job, a wind-up
// (demo/modules/cast_time_attack), a recurring cooldown cycle
// (demo/modules/auto_attack). A request that *does* resolve synchronously
// (most of them) is the degenerate case where Started and Completed happen
// in the same call - RequestResult (accept/reject) plus whatever event a
// handler publishes already models that; this enum and the two functions
// below only earn their keep for the requests where Ongoing is a real,
// observable state that persists across ticks or across an external
// round-trip.
enum class ActionState : std::uint8_t { Started, Ongoing, Cancelled, Completed };

// A property is Cancellable when it carries both pieces advance_action
// below needs: action_state (the lifecycle position itself) and
// cancel_requested (a pending cancellation signal, set by request_cancel
// and consumed - checked and cleared - by the very next advance_action
// call). Deliberately a structural concept, not a base class or a runtime
// interface table (spec §5: "Contract satisfaction is a compile-time
// fact... never a runtime interface table") - the same device
// atlas::Composable<T> already is for property composition.
//
// Not something atlas-cgen's generated contracts can express directly yet:
// the manifest type system has no enum field type (only float/int32/
// uint64/bool/EntityRef), so a capability wanting a Cancellable property
// keeps action_state/cancel_requested in a hand-written companion
// structure alongside its generated property - the same "capability's own
// private per-entity bookkeeping" shape armor::ContributionRegistry/
// movement::ContributionRegistry already establish, not a new pattern
// invented for this.
template <typename T>
concept Cancellable = requires(T action) {
    { action.action_state } -> std::convertible_to<ActionState>;
    { action.cancel_requested } -> std::convertible_to<bool>;
};

// Requests cancellation: sets cancel_requested, nothing else. Deliberately
// does not touch action_state itself - the actual transition to Cancelled
// only happens inside advance_action, at the one well-defined point every
// action already passes through each tick, not the instant a cancelling
// event (movement, a crowd-control effect, ...) arrives. This is what
// keeps "the runtime handles cancel first" true structurally rather than
// as a convention every caller has to remember: cancellation is queued
// here, and only ever acted on inside advance_action, before that call's
// own on_advance ever runs.
template <typename T>
    requires Cancellable<T>
void request_cancel(T& action) {
    action.cancel_requested = true;
}

// Advances one action by exactly one step, checking for a pending
// cancellation *before* running any of the action's own per-tick logic:
//
// - Already Cancelled or Completed: a no-op. A terminal action stays
//   terminal until whoever owns it explicitly restarts it (e.g. a fresh
//   BeginCast) - advance_action never resurrects one on its own.
// - cancel_requested is set: clears the flag, transitions to Cancelled,
//   runs on_cancel, and returns - on_advance is not invoked at all this
//   call. on_cancel is the capability's own hook for whatever cancellation
//   specifically means to it (cast_time_attack goes idle; auto_attack
//   resets its cooldown to a full cycle instead) - most actions need
//   nothing beyond the state transition itself, so an empty lambda is a
//   perfectly ordinary on_cancel.
// - Otherwise: runs on_advance, the action's own normal per-tick
//   progression (including deciding for itself when to transition to
//   Ongoing or Completed - advance_action has no opinion on that, the same
//   way resolve_additive/resolve_multiplicative have no opinion on what a
//   property represents).
template <typename T, typename OnCancel, typename OnAdvance>
    requires Cancellable<T> && std::invocable<OnCancel, T&> && std::invocable<OnAdvance, T&>
void advance_action(T& action, OnCancel&& on_cancel, OnAdvance&& on_advance) {
    if (action.action_state == ActionState::Cancelled || action.action_state == ActionState::Completed) {
        return;
    }

    if (action.cancel_requested) {
        action.cancel_requested = false;
        action.action_state = ActionState::Cancelled;
        std::forward<OnCancel>(on_cancel)(action);
        return;
    }

    std::forward<OnAdvance>(on_advance)(action);
}

} // namespace atlas::runtime
