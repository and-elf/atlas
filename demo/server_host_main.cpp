#include "atlas/core/time.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "orb_host.hpp"

// server-host (issue #277): the authoritative process of the three-way
// orb-demo split. Holds the orb's real Position/MovementSpeed state and
// (once #278 wires real transport) would validate and apply move/retexture
// requests arriving from editor-host - today it only spawns the orb and
// ticks in place, since there is no transport yet for anything to arrive
// over. Deliberately headless: links no atlas-render/atlas-input, matching
// §13's "a headless server host must never gain a dependency on any of
// [the optional presentation libraries]".
int main(int argc, char** argv) {
    std::optional<std::uint64_t> tick_limit;
    if (argc == 3 && std::string_view(argv[1]) == "--ticks") {
        tick_limit = std::strtoull(argv[2], nullptr, 10);
    } else if (argc != 1) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
        std::fprintf(stderr, "usage: server-host [--ticks N]\n");
        return 2;
    }

    atlas::demo::OrbApp app{/*has_authority=*/true};
    const auto orb = atlas::demo::spawn_orb(app);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
    std::fprintf(stdout,
                 "server-host: authoritative orb spawned, ticking at %llu Hz\n",
                 static_cast<unsigned long long>(atlas::core::Time::ticks_per_second));
    std::fflush(stdout);

    atlas::demo::run_paced(app, tick_limit, [&](std::uint64_t tick) {
        if (tick % atlas::core::Time::ticks_per_second != 0) {
            return;
        }
        const auto position = app.ctx.get<atlas::movement::Position>(orb);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
        std::fprintf(stdout,
                     "server-host: t=%.1fs orb at (%.2f, %.2f)\n",
                     static_cast<double>(tick) / static_cast<double>(atlas::core::Time::ticks_per_second),
                     static_cast<double>(position->get().x),
                     static_cast<double>(position->get().y));
        std::fflush(stdout);
    });

    return 0;
}
