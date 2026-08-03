#pragma once

#include "atlas/runtime/context.hpp"
#include "atlas/runtime/host.hpp"

#include <cstdint>
#include <functional>

namespace atlas::demo {

// Drives exactly `tick_count` real ticks against host/ctx via
// atlas::advance_tick (issue #70) - the first place in this codebase that
// actually exercises Host::run_tick()/Context::end_tick() end to end for
// demo's composed capabilities, rather than the delta_ticks-parameterized
// request pattern every demo/tests/*.cpp scenario uses instead (see
// demo/README.md). Deliberately mechanism-only: it advances the tick
// boundary and nothing else - no gameplay logic is scheduled against any
// stage here, matching this issue's own "just composition, no new gameplay
// semantics" scope.
//
// on_tick, if given, is invoked once per tick with the 1-based tick number
// just completed. main()'s own real-time pacing and heartbeat logging hang
// off this callback rather than living inside this function, which keeps
// run_ticks itself deterministic and fully unit-testable without any
// wall-clock dependency (demo/tests/host_loop_test.cpp).
void run_ticks(runtime::Host& host,
               Context& ctx,
               std::uint64_t tick_count,
               const std::function<void(std::uint64_t)>& on_tick = {});

} // namespace atlas::demo
