#include "atlas/core/time.hpp"
#include "atlas/request/dispatch.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <string_view>

#include "orb_host.hpp"

// editor-host (issue #277, minimal slice - real input/UI wiring is #279):
// the process that actually moves the orb. Locally issues a real Move
// request each tick (the same request/dispatcher path a network-delivered
// request from a real editor UI would go through, per §10 The Editor Is A
// Client - the editor composes the same runtime/capability libraries as any
// other host, no privileged execution path) rather than mutating Position
// directly, so this genuinely exercises the request surface #278's
// transport will later carry across the real process boundary. Direction
// traces a slow circle - an arbitrary, visibly-moving stand-in for real
// input, not a design requirement.
//
// Constructed with has_authority=true today ONLY as a stand-in: on_move
// (movement.cpp) rejects any Move without authority (spec §6 - the editor
// must never itself be authoritative), which is exactly correct once #278
// wires this request to travel to the real, separate, authoritative
// server-host process instead of resolving against editor-host's own local
// Context. Until that transport exists there is no server for editor-host to
// send anything to, so resolving locally-with-authority is this round's own
// honestly-scoped placeholder - flip this to false and route through #278's
// transport instead of dispatching locally once it lands.
int main(int argc, char** argv) {
    std::optional<std::uint64_t> tick_limit;
    if (argc == 3 && std::string_view(argv[1]) == "--ticks") {
        tick_limit = std::strtoull(argv[2], nullptr, 10);
    } else if (argc != 1) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
        std::fprintf(stderr, "usage: editor-host [--ticks N]\n");
        return 2;
    }

    atlas::demo::OrbApp app{/*has_authority=*/true};
    const auto orb = atlas::demo::spawn_orb(app);

    atlas::request::Dispatcher<atlas::movement::Move> move_dispatcher;
    move_dispatcher.register_handler(atlas::movement::on_move);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
    std::fprintf(stdout,
                 "editor-host: moving orb, ticking at %llu Hz\n",
                 static_cast<unsigned long long>(atlas::core::Time::ticks_per_second));
    std::fflush(stdout);

    atlas::demo::run_paced(app, tick_limit, [&](std::uint64_t tick) {
        const double angle = static_cast<double>(tick) /
                             static_cast<double>(atlas::core::Time::ticks_per_second) *
                             (2.0 * std::numbers::pi / 4.0); // one full circle every 4 simulated seconds
        const auto move_result =
            move_dispatcher.dispatch(app.ctx,
                                     atlas::movement::Move{
                                         .target = orb,
                                         .direction_x = static_cast<float>(std::cos(angle)),
                                         .direction_y = static_cast<float>(std::sin(angle)),
                                         .delta_ticks = 1,
                                     });
        (void)move_result; // has_authority=true above guarantees acceptance; nothing else this round could
                           // reject it

        if (tick % atlas::core::Time::ticks_per_second != 0) {
            return;
        }
        const auto position = app.ctx.get<atlas::movement::Position>(orb);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
        std::fprintf(stdout,
                     "editor-host: t=%.1fs orb at (%.2f, %.2f)\n",
                     static_cast<double>(tick) / static_cast<double>(atlas::core::Time::ticks_per_second),
                     static_cast<double>(position->get().x),
                     static_cast<double>(position->get().y));
        std::fflush(stdout);
    });

    return 0;
}
