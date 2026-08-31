#include "orb_host.hpp"

#include "atlas/core/time.hpp"

#include <chrono>
#include <thread>

#include "host_loop.hpp"

namespace atlas::demo {

void run_paced(OrbApp& app,
               std::optional<std::uint64_t> tick_limit,
               const std::function<void(std::uint64_t)>& on_tick) {
    using Clock = std::chrono::steady_clock;
    const auto tick_period = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / static_cast<double>(core::Time::ticks_per_second)));
    auto next_tick_at = Clock::now();
    std::uint64_t ticks_run = 0;

    while (!tick_limit.has_value() || ticks_run < *tick_limit) {
        run_ticks(app.host, app.ctx, 1, [&](std::uint64_t /*tick*/) {
            ++ticks_run;
            on_tick(ticks_run);
        });

        if (tick_limit.has_value()) {
            // Bounded/smoke-test mode: run as fast as possible, no pacing.
            continue;
        }
        next_tick_at += tick_period;
        std::this_thread::sleep_until(next_tick_at);
    }
}

} // namespace atlas::demo
