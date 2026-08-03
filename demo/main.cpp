#include "atlas/core/time.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/runtime/host.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

#include "demo_host.host.hpp"
#include "host_loop.hpp"

// demo-host (issue #70): a real, running in-process host executable
// composing demo/modules' existing capabilities - pure composition, no new
// gameplay semantics (demo/README.md's scope boundary), matching #70's own
// scope text exactly. This is the foundation the companion "wire in
// atlas-input/atlas-render/atlas-audio" issue (#71) builds on: today this
// process composes a real Host+Context (the same mechanism
// demo/tests/simulated_host.hpp wraps for tests, generated from
// demo_host.host.yaml the same way simulated_host.host.yaml is) and drives a
// real tick loop via demo::run_ticks/atlas::advance_tick - the first place
// in this codebase that actually exercises Host::schedule()/run_tick() for
// demo capabilities end to end, rather than the delta_ticks-parameterized
// direct-dispatch pattern every demo/tests/*.cpp scenario uses instead. No
// entities are created and no requests are dispatched here: with no real
// input/render/audio wired in yet (#71), there is nothing yet driving player
// intent or observing composed state, so an empty, ticking host is the
// honestly-scoped thing to build - #71 is what gives this loop something to
// actually do each tick.
//
// Not unit tested here - CLI entry points (argv parsing, signal handling,
// real-time pacing) are integration-level, the same convention
// tools/atlas-cgen/src/main.cpp documents and cmake/CodeCoverage.cmake
// excludes this file the same way. The logic this file wires together
// (demo::run_ticks) is unit tested directly in demo/tests/host_loop_test.cpp.
namespace {

// A mutable global is exactly what a signal handler needs: SIGINT/SIGTERM
// deliver into handle_stop_signal asynchronously, with no context pointer to
// route through, so this is the standard C/C++ idiom for the flag it writes
// to (volatile sig_atomic_t - the one type the standard guarantees is safe
// to read/write from a signal handler without a data race), not an
// oversight this project's usual "no non-const globals" bar would otherwise
// require avoiding.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile std::sig_atomic_t g_stop_requested = 0;

void handle_stop_signal(int /*signal*/) {
    g_stop_requested = 1;
}

// std::fprintf, not std::println: verified (not hypothetical) that GCC
// 13.3.0 - this sandbox's own compiler, matching this project's actual CI
// toolchain (CLAUDE.md) - has no <print> header at all (added in GCC 14),
// the same class of "documented in CLAUDE.md, not a style choice" toolchain
// gap as the project's existing std::expected/libstdc++ note. Every
// std::fprintf call site below carries the same NOLINT for this reason.
void print_usage() {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
    std::fprintf(stderr,
                 "usage: demo-host [--ticks N]\n"
                 "  --ticks N   run exactly N ticks as fast as possible, then exit\n"
                 "              (bounded/smoke-test mode; default: run at real time,\n"
                 "              ticks_per_second Hz, until Ctrl+C)\n");
}

} // namespace

int main(int argc, char** argv) {
    std::optional<std::uint64_t> tick_limit;
    if (argc == 3 && std::string_view(argv[1]) == "--ticks") {
        tick_limit = std::strtoull(argv[2], nullptr, 10);
    } else if (argc != 1) {
        print_usage();
        return 2;
    }

    std::signal(SIGINT, handle_stop_signal);
    std::signal(SIGTERM, handle_stop_signal);

    std::optional<atlas::stage::StageSequence> sequence =
        atlas::stage::StageSequence::create({atlas::stage::StageId{"Simulation"}});
    if (!sequence.has_value()) {
        // Only reachable if the fixed, single-entry stage list above ever
        // grows a duplicate StageId - StageSequence::create's own documented
        // failure mode. Checked explicitly (never an unchecked *sequence)
        // rather than assumed, since main() is a real process entry point,
        // not test code.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
        std::fprintf(stderr, "demo-host: invalid stage sequence\n");
        return 1;
    }
    atlas::runtime::Host host{std::move(*sequence), /*has_authority=*/true};
    atlas::Context ctx{host};
    atlas::DemoRuntimeHost composition;
    register_property_stores(ctx, composition);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
    std::fprintf(stdout,
                 "demo-host: composed, ticking at %llu Hz - Ctrl+C to stop\n",
                 static_cast<unsigned long long>(atlas::core::Time::ticks_per_second));
    std::fflush(stdout);

    using Clock = std::chrono::steady_clock;
    const auto tick_period = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / static_cast<double>(atlas::core::Time::ticks_per_second)));
    auto next_tick_at = Clock::now();
    std::uint64_t ticks_run = 0;

    while (g_stop_requested == 0 && (!tick_limit.has_value() || ticks_run < *tick_limit)) {
        atlas::demo::run_ticks(host, ctx, 1, [&ticks_run](std::uint64_t) {
            ++ticks_run;
            if (ticks_run % atlas::core::Time::ticks_per_second == 0) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
                std::fprintf(stdout,
                             "demo-host: tick %llu (t=%.1fs)\n",
                             static_cast<unsigned long long>(ticks_run),
                             static_cast<double>(ticks_run) /
                                 static_cast<double>(atlas::core::Time::ticks_per_second));
                std::fflush(stdout);
            }
        });

        if (tick_limit.has_value()) {
            // Bounded/smoke-test mode: run as fast as possible, no real-time
            // pacing between ticks.
            continue;
        }
        next_tick_at += tick_period;
        std::this_thread::sleep_until(next_tick_at);
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
    std::fprintf(
        stdout, "demo-host: shutting down after %llu tick(s)\n", static_cast<unsigned long long>(ticks_run));
    return 0;
}
