#include "atlas/core/time.hpp"
#include "atlas/replication/unix_socket_transport.hpp"

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string_view>

#include "orb_host.hpp"
#include "orb_transport.hpp"

// client-host (issue #278, building on #277's process split): the
// observing process - not authoritative, never dispatches a request itself.
// Binds a real atlas::replication::UnixSocketTransport at a well-known path
// (orb_transport.hpp) and applies whatever PositionUpdate server-host
// broadcasts each tick directly to its own local PropertyStore (genuinely
// decoded from the wire, not shared memory) - this is what #280's real
// rendering will read from once it lands; today this file only logs it.
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
    atlas::replication::UnixSocketTransport transport{atlas::demo::client_socket_path()};
    std::optional<atlas::EntityRef> observed_orb;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
    std::fprintf(stdout,
                 "client-host: observing orb on '%s', ticking at %llu Hz\n",
                 atlas::demo::client_socket_path().string().c_str(),
                 static_cast<unsigned long long>(atlas::core::Time::ticks_per_second));
    std::fflush(stdout);

    atlas::demo::run_paced(app, tick_limit, [&](std::uint64_t tick) {
        for (const auto& received : transport.poll_received()) {
            const auto update = atlas::demo::decode_position_update(received.payload);
            if (!update.has_value()) {
                continue; // malformed/unrecognized payload - ignored, not fatal (transport contract: lossy)
            }
            app.position_store.set(update->entity, update->position);
            observed_orb = update->entity;
        }

        if (tick % atlas::core::Time::ticks_per_second != 0) {
            return;
        }
        if (!observed_orb.has_value()) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
            std::fprintf(stdout,
                         "client-host: t=%.1fs no PositionUpdate received yet\n",
                         static_cast<double>(tick) /
                             static_cast<double>(atlas::core::Time::ticks_per_second));
            std::fflush(stdout);
            return;
        }
        const auto position = app.ctx.get<atlas::movement::Position>(*observed_orb);
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
