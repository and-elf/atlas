#include "atlas/core/time.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "orb_host.hpp"

// client-host (issue #277): the observing process of the three-way
// orb-demo split - not authoritative, would render whatever the server
// replicates once #278/#280 wire real transport/rendering. Today it only
// spawns its own local orb and ticks it in place (no replication exists
// yet, so there is nothing for it to observe from the server), proving
// only that a real, separate, render-capable process boundary exists.
int main(int argc, char** argv) {
    std::optional<std::uint64_t> tick_limit;
    if (argc == 3 && std::string_view(argv[1]) == "--ticks") {
        tick_limit = std::strtoull(argv[2], nullptr, 10);
    } else if (argc != 1) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
        std::fprintf(stderr, "usage: client-host [--ticks N]\n");
        return 2;
    }

    atlas::demo::OrbApp app{/*has_authority=*/false};
    const auto orb = atlas::demo::spawn_orb(app);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
    std::fprintf(stdout,
                 "client-host: observing orb, ticking at %llu Hz\n",
                 static_cast<unsigned long long>(atlas::core::Time::ticks_per_second));
    std::fflush(stdout);

    atlas::demo::run_paced(app, tick_limit, [&](std::uint64_t tick) {
        if (tick % atlas::core::Time::ticks_per_second != 0) {
            return;
        }
        const auto position = app.ctx.get<atlas::movement::Position>(orb);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
        std::fprintf(stdout,
                     "client-host: t=%.1fs orb at (%.2f, %.2f)\n",
                     static_cast<double>(tick) / static_cast<double>(atlas::core::Time::ticks_per_second),
                     static_cast<double>(position->get().x),
                     static_cast<double>(position->get().y));
        std::fflush(stdout);
    });

    return 0;
}
