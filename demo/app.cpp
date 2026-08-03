#include "app.hpp"

#include "atlas/core/time.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <thread>

#include "host_loop.hpp"

namespace atlas::demo {

namespace {

// A mutable global is exactly what a signal handler needs: SIGINT/SIGTERM
// deliver asynchronously, with no context pointer to route through, so this
// is the standard C/C++ idiom for the flag it writes to (volatile
// sig_atomic_t - the one type the standard guarantees is safe to read/write
// from a signal handler without a data race), not an oversight this
// project's usual "no non-const globals" bar would otherwise require
// avoiding. A single process-wide flag (rather than per-App routing) is
// sufficient here: a real process only ever runs one App, and
// App::stop_requested() is virtual specifically so a derived class can
// substitute an entirely different stop condition without this flag or the
// signal handlers below ever being involved.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile std::sig_atomic_t g_stop_requested = 0;

void handle_os_stop_signal(int /*signal*/) {
    g_stop_requested = 1;
}

void print_usage() {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
    std::fprintf(stderr,
                 "usage: demo-host [--ticks N]\n"
                 "  --ticks N   run exactly N ticks as fast as possible, then exit\n"
                 "              (bounded/smoke-test mode; default: run at real time,\n"
                 "              ticks_per_second Hz, until Ctrl+C)\n");
}

// The fixed, single-stage sequence every App composes against - identical
// to demo/tests/simulated_host.hpp's own make_sequence(). Duplication is
// structurally impossible with a fixed one-entry list, so a failure here can
// only mean a future edit introduced a genuine duplicate StageId - a setup
// mistake, not a runtime condition, hence throwing rather than propagating
// an optional (matching atlas::Context::get<T>()'s own "setup mistake"
// throwing precedent).
stage::StageSequence make_stage_sequence() {
    std::optional<stage::StageSequence> sequence =
        stage::StageSequence::create({stage::StageId{"Simulation"}});
    if (!sequence.has_value()) {
        throw std::logic_error("atlas::demo::App: duplicate StageId in its own fixed stage list");
    }
    return std::move(*sequence);
}

} // namespace

App::App(int argc, char** argv) : host_(make_stage_sequence(), /*has_authority=*/true), ctx_(host_) {
    if (argc == 3 && std::string_view(argv[1]) == "--ticks") {
        tick_limit_ = std::strtoull(argv[2], nullptr, 10);
    } else if (argc != 1) {
        print_usage();
        parsed_ok_ = false;
    }

    register_property_stores(ctx_, composition_);
    std::signal(SIGINT, handle_os_stop_signal);
    std::signal(SIGTERM, handle_os_stop_signal);
}

int App::run() {
    if (!parsed_ok_) {
        return 2;
    }

    // std::fprintf, not std::println: verified (not hypothetical) that GCC
    // 13.3.0 - this sandbox's own compiler, matching this project's actual
    // CI toolchain (CLAUDE.md) - has no <print> header at all (added in GCC
    // 14), the same class of "documented in CLAUDE.md, not a style choice"
    // toolchain gap as the project's existing std::expected/libstdc++ note.
    // Every std::fprintf call site in this file carries the same NOLINT for
    // this reason.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
    std::fprintf(stdout,
                 "demo-host: composed, ticking at %llu Hz - Ctrl+C to stop\n",
                 static_cast<unsigned long long>(core::Time::ticks_per_second));
    std::fflush(stdout);

    using Clock = std::chrono::steady_clock;
    const auto tick_period = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / static_cast<double>(core::Time::ticks_per_second)));
    auto next_tick_at = Clock::now();
    std::uint64_t ticks_run = 0;

    while (!stop_requested() && (!tick_limit_.has_value() || ticks_run < *tick_limit_)) {
        run_ticks(host_, ctx_, 1, [this, &ticks_run](std::uint64_t) {
            ++ticks_run;
            on_tick(ticks_run);
        });

        if (tick_limit_.has_value()) {
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

void App::on_tick(std::uint64_t tick) {
    if (tick % core::Time::ticks_per_second == 0) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
        std::fprintf(stdout,
                     "demo-host: tick %llu (t=%.1fs)\n",
                     static_cast<unsigned long long>(tick),
                     static_cast<double>(tick) / static_cast<double>(core::Time::ticks_per_second));
        std::fflush(stdout);
    }
}

bool App::stop_requested() const {
    return g_stop_requested != 0;
}

} // namespace atlas::demo
