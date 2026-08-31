#include "atlas/core/time.hpp"
#include "atlas/replication/unix_socket_transport.hpp"
#include "atlas/request/dispatch.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "orb_host.hpp"
#include "orb_transport.hpp"

// server-host (issue #278, building on #277's process split): the
// authoritative process. Binds a real atlas::replication::UnixSocketTransport
// at a well-known path (orb_transport.hpp), decodes any pending MoveMessage
// each tick and dispatches it as a real movement::Move request against its
// own authoritative Context (spec §6 - the handler itself validates/rejects,
// this file never coerces a request into something valid), then broadcasts
// the orb's resulting Position to client-host and editor-host's well-known
// paths every tick. Deliberately headless: links no atlas-render/
// atlas-input, matching §13's "a headless server host must never gain a
// dependency on any of [the optional presentation libraries]".
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

    atlas::replication::UnixSocketTransport transport{atlas::demo::server_socket_path()};

    atlas::request::Dispatcher<atlas::movement::Move> move_dispatcher;
    move_dispatcher.register_handler(atlas::movement::on_move);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
    std::fprintf(stdout,
                 "server-host: authoritative orb spawned, listening on '%s', ticking at %llu Hz\n",
                 atlas::demo::server_socket_path().string().c_str(),
                 static_cast<unsigned long long>(atlas::core::Time::ticks_per_second));
    std::fflush(stdout);

    atlas::demo::run_paced(app, tick_limit, [&](std::uint64_t tick) {
        for (const auto& received : transport.poll_received()) {
            const auto move = atlas::demo::decode_move(received.payload);
            if (!move.has_value()) {
                continue; // malformed/unrecognized payload - ignored, not fatal (transport contract: lossy)
            }
            (void)move_dispatcher.dispatch(app.ctx,
                                           atlas::movement::Move{
                                               .target = move->target,
                                               .direction_x = move->direction_x,
                                               .direction_y = move->direction_y,
                                               .delta_ticks = move->delta_ticks,
                                           });
        }

        const auto position = app.ctx.get<atlas::movement::Position>(orb);
        const auto update = atlas::demo::encode_position_update(
            atlas::demo::PositionUpdateMessage{.entity = orb, .position = position->get()});
        (void)transport.send(atlas::demo::client_socket_path(), update);
        (void)transport.send(atlas::demo::editor_socket_path(), update);

        if (tick % atlas::core::Time::ticks_per_second != 0) {
            return;
        }
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
