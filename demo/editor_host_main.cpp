#include "atlas/core/time.hpp"
#include "atlas/replication/unix_socket_transport.hpp"
#include "atlas/resource/resource_id.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <optional>
#include <string_view>

#include "orb_host.hpp"
#include "orb_transport.hpp"

// editor-host (issues #278/#279, building on #277's process split): the
// process that moves and recolors the orb. Binds a real
// atlas::replication::UnixSocketTransport at a well-known path
// (orb_transport.hpp) and sends a real Move every tick plus a Renderable
// message every kRetextureIntervalTicks - the wire encoding of the same
// requests a real editor UI would issue (per §10 The Editor Is A Client -
// the editor never resolves either locally, only server-host's own
// authoritative Context/renderable_store may). Move direction traces a slow
// circle and retexture cycles kOrbMaterialPalette (orb_host.hpp) - both
// arbitrary, visibly-changing stand-ins for real input, not a design
// requirement. Real windowed input (atlas-input, a real keyboard/mouse
// source) is deliberately deferred to #280: every existing real-input
// backend in this codebase (Sdl3RawSignalSource) is paired with a real
// window (Sdl3SharedWindow, demo/main.cpp's own precedent), and #280 is what
// gives this process a real window to pair one with - inventing a
// windowless input source here would be a parallel, throwaway mechanism.
//
// Never spawns its own local orb: the only real orb is server-host's. This
// process waits to learn its EntityRef from server-host's own broadcast
// Position message (the only source of truth for what to target) before it
// starts sending Move/Renderable messages - not a hardcoded EntityRef{0, 0}
// assumption, which would only coincidentally hold for this narrow
// single-orb demo.
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
            if (const auto position = atlas::demo::decode_position(received.payload); position.has_value()) {
                orb = position->entity;
                app.position_store.set(position->entity, position->position);
                continue;
            }
            if (const auto renderable = atlas::demo::decode_renderable(received.payload);
                renderable.has_value()) {
                app.renderable_store.set(renderable->entity, renderable->renderable);
                continue;
            }
            // malformed/unrecognized payload - ignored, not fatal (transport contract: lossy)
        }

        if (orb.has_value()) {
            const double angle = static_cast<double>(tick) /
                                 static_cast<double>(atlas::core::Time::ticks_per_second) *
                                 (2.0 * std::numbers::pi / 4.0); // one full circle every 4 simulated seconds
            const auto move_payload = atlas::demo::encode_move(atlas::movement::Move{
                .target = *orb,
                .direction_x = static_cast<float>(std::cos(angle)),
                .direction_y = static_cast<float>(std::sin(angle)),
                .delta_ticks = 1,
            });
            (void)transport.send(atlas::demo::server_socket_path(), move_payload);

            // Cycles kOrbMaterialPalette every kRetextureIntervalTicks - an
            // arbitrary, visibly-changing stand-in for a real "recolor"
            // input action, not a design requirement (see this file's own
            // header comment on why real input stays #280's scope).
            constexpr std::uint64_t kRetextureIntervalTicks =
                2 * atlas::core::Time::ticks_per_second; // every 2 simulated seconds
            if (tick % kRetextureIntervalTicks == 0) {
                const auto palette_index =
                    (tick / kRetextureIntervalTicks) % atlas::demo::kOrbMaterialPalette.size();
                const auto renderable_payload = atlas::demo::encode_renderable(atlas::demo::RenderableMessage{
                    .entity = *orb,
                    .renderable =
                        atlas::render::Renderable{
                            .mesh = {},
                            .material =
                                atlas::ResourceId::from_name(atlas::demo::kOrbMaterialPalette[palette_index]),
                        },
                });
                (void)transport.send(atlas::demo::server_socket_path(), renderable_payload);
            }
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
