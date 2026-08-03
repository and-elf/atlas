#include "host_loop.hpp"

namespace atlas::demo {

void run_ticks(runtime::Host& host,
               Context& ctx,
               std::uint64_t tick_count,
               const std::function<void(std::uint64_t)>& on_tick,
               const std::function<void(std::uint64_t)>& pre_tick) {
    for (std::uint64_t tick = 1; tick <= tick_count; ++tick) {
        if (pre_tick) {
            pre_tick(tick);
        }
        advance_tick(host, ctx);
        if (on_tick) {
            on_tick(tick);
        }
    }
}

} // namespace atlas::demo
