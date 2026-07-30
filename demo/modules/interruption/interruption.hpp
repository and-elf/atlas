#pragma once

// Generated at build time from
// demo/modules/interruption/interruption.capability.yaml (see
// demo/CMakeLists.txt) - the ActionInterrupted contract. No properties or
// requests, and no hand-written .cpp: this capability is purely a shared
// vocabulary event, published by whatever capability decides an entity's
// current action should stop (a crowd-control effect like a stun or
// disorient - not yet built in this demo, see this directory's README
// section - or, indirectly, movement, via each interruptible capability's
// own opt-in subscription to movement::PositionChanged rather than through
// this event at all) and subscribed to by whatever capability has
// cancellable in-progress state of its own (cast_time_attack, auto_attack).
//
// Deliberately capability-agnostic: ActionInterrupted carries only the
// affected entity, nothing about why - the same "Atlas defines execution,
// capabilities define behavior" separation every other event in this demo
// already respects (spec §2). A subscriber decides for itself what
// "interrupted" means for its own state (cast_time_attack cancels outright;
// auto_attack resets its swing timer) - this capability has no opinion and
// no state of its own to mutate.
#include "interruption.capability.hpp"
