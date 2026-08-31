#include "atlas/core/time.hpp"
#include "atlas/replication/unix_socket_transport.hpp"

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string_view>

#include "orb_host.hpp"
#include "orb_transport.hpp"

// client-host (issues #278/#279, building on #277's process split): the
// observing process - not authoritative, never dispatches a request itself.
// Binds a real atlas::replication::UnixSocketTransport at a well-known path
// (orb_transport.hpp) and applies whatever Position/Renderable broadcast
// server-host sends each tick directly to its own local PropertyStores
// (genuinely decoded from the wire, not shared memory) - this is what
// #280's real rendering will read from once it lands; today this file only
// logs it.
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
            if (const auto position = atlas::demo::decode_position(received.payload); position.has_value()) {
                app.position_store.set(position->entity, position->position);
                observed_orb = position->entity;
                continue;
            }
            if (const auto renderable = atlas::demo::decode_renderable(received.payload);
                renderable.has_value()) {
                app.renderable_store.set(renderable->entity, renderable->renderable);
                continue;
            }
            // malformed/unrecognized payload - ignored, not fatal (transport contract: lossy)
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
        const auto renderable = app.renderable_store.get(*observed_orb);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
        std::fprintf(stdout,
                     "client-host: t=%.1fs orb at (%.2f, %.2f), material=0x%llx\n",
                     static_cast<double>(tick) / static_cast<double>(atlas::core::Time::ticks_per_second),
                     static_cast<double>(position->get().x),
                     static_cast<double>(position->get().y),
                     renderable.has_value()
                         ? static_cast<unsigned long long>(renderable->get().material.value)
                         : 0ULL);
        std::fflush(stdout);
    });

    return 0;
}
