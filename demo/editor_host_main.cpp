#include "atlas/core/time.hpp"
#include "atlas/replication/unix_socket_transport.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <optional>
#include <string_view>

#include "orb_host.hpp"
#include "orb_transport.hpp"

// editor-host (issue #278, building on #277's process split; real input/UI
// wiring is still #279): the process that moves the orb. Binds a real
// atlas::replication::UnixSocketTransport at a well-known path
// (orb_transport.hpp) and sends a real MoveMessage every tick - the wire
// encoding of the same movement::Move request a real editor UI would issue
// (per §10 The Editor Is A Client - the editor never resolves this locally,
// only server-host's own authoritative Context may, spec §6). Direction
// traces a slow circle - an arbitrary, visibly-moving stand-in for real
// input, not a design requirement.
//
// Never spawns its own local orb: the only real orb is server-host's. This
// process waits to learn its EntityRef from server-host's own broadcast
// PositionUpdate (the only source of truth for what to target) before it
// starts sending Move messages - not a hardcoded EntityRef{0, 0} assumption,
// which would only coincidentally hold for this narrow single-orb demo.
int main(int argc, char** argv) {
    std::optional<std::uint64_t> tick_limit;
    if (argc == 3 && std::string_view(argv[1]) == "--ticks") {
        tick_limit = std::strtoull(argv[2], nullptr, 10);
    } else if (argc != 1) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
        std::fprintf(stderr, "usage: editor-host [--ticks N]\n");
        return 2;
    }

    atlas::demo::OrbApp app{/*has_authority=*/false};
    atlas::replication::UnixSocketTransport transport{atlas::demo::editor_socket_path()};
    std::optional<atlas::EntityRef> orb;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
    std::fprintf(stdout,
                 "editor-host: waiting to learn the orb's identity from server-host, ticking at %llu Hz\n",
                 static_cast<unsigned long long>(atlas::core::Time::ticks_per_second));
    std::fflush(stdout);

    atlas::demo::run_paced(app, tick_limit, [&](std::uint64_t tick) {
        for (const auto& received : transport.poll_received()) {
            const auto update = atlas::demo::decode_position_update(received.payload);
            if (update.has_value()) {
                orb = update->entity;
                app.position_store.set(update->entity, update->position);
            }
        }

        if (orb.has_value()) {
            const double angle = static_cast<double>(tick) /
                                 static_cast<double>(atlas::core::Time::ticks_per_second) *
                                 (2.0 * std::numbers::pi / 4.0); // one full circle every 4 simulated seconds
            const auto payload = atlas::demo::encode_move(atlas::demo::MoveMessage{
                .target = *orb,
                .direction_x = static_cast<float>(std::cos(angle)),
                .direction_y = static_cast<float>(std::sin(angle)),
                .delta_ticks = 1,
            });
            (void)transport.send(atlas::demo::server_socket_path(), payload);
        }

        if (tick % atlas::core::Time::ticks_per_second != 0) {
            return;
        }
        if (!orb.has_value()) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
            std::fprintf(stdout,
                         "editor-host: t=%.1fs orb identity not yet known\n",
                         static_cast<double>(tick) /
                             static_cast<double>(atlas::core::Time::ticks_per_second));
            std::fflush(stdout);
            return;
        }
        const auto position = app.ctx.get<atlas::movement::Position>(*orb);
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
