#include "atlas/core/time.hpp"
#include "atlas/replication/unix_socket_transport.hpp"
#include "atlas/request/dispatch.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "orb_host.hpp"
#include "orb_transport.hpp"

// server-host (issues #278/#279, building on #277's process split): the
// authoritative process. Binds a real atlas::replication::UnixSocketTransport
// at a well-known path (orb_transport.hpp), decodes any pending Move/
// Renderable message each tick and applies it to its own authoritative
// state - Move dispatches through the real request/dispatcher path (spec
// §6, the handler itself validates/rejects), Renderable mutates
// renderable_store directly (no capability manifest exists for appearance,
// see orb_host.hpp's own comment) - then broadcasts the orb's resulting
// Position/Renderable to client-host and editor-host's well-known paths
// every tick. Deliberately headless: links no atlas-render/atlas-input,
// matching §13's "a headless server host must never gain a dependency on
// any of [the optional presentation libraries]".
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
            if (const auto move = atlas::demo::decode_move(received.payload); move.has_value()) {
                (void)move_dispatcher.dispatch(app.ctx, *move);
                continue;
            }
            if (const auto renderable = atlas::demo::decode_renderable(received.payload);
                renderable.has_value()) {
                app.renderable_store.set(renderable->entity, renderable->renderable);
                continue;
            }
            // malformed/unrecognized payload - ignored, not fatal (transport contract: lossy)
        }

        const auto position = app.ctx.get<atlas::movement::Position>(orb);
        const auto position_payload = atlas::demo::encode_position(
            atlas::demo::PositionMessage{.entity = orb, .position = position->get()});
        (void)transport.send(atlas::demo::client_socket_path(), position_payload);
        (void)transport.send(atlas::demo::editor_socket_path(), position_payload);

        const auto renderable = app.renderable_store.get(orb);
        const auto renderable_payload = atlas::demo::encode_renderable(
            atlas::demo::RenderableMessage{.entity = orb, .renderable = renderable->get()});
        (void)transport.send(atlas::demo::client_socket_path(), renderable_payload);
        (void)transport.send(atlas::demo::editor_socket_path(), renderable_payload);

        if (tick % atlas::core::Time::ticks_per_second != 0) {
            return;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
        std::fprintf(stdout,
                     "server-host: t=%.1fs orb at (%.2f, %.2f), material=0x%llx\n",
                     static_cast<double>(tick) / static_cast<double>(atlas::core::Time::ticks_per_second),
                     static_cast<double>(position->get().x),
                     static_cast<double>(position->get().y),
                     static_cast<unsigned long long>(renderable->get().material.value));
        std::fflush(stdout);
    });

    return 0;
}
