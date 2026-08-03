#pragma once

#include "atlas/runtime/context.hpp"
#include "atlas/runtime/host.hpp"

#include <cstdint>
#include <optional>

#include "demo_host.host.hpp"

namespace atlas::demo {

// A Qt-Application-style bootstrap (PR #185 review feedback): wraps
// everything demo-host's own main() used to do by hand - constructing a
// real Host+Context+DemoRuntimeHost composition, running a real-time-paced
// tick loop via run_ticks/advance_tick (host_loop.hpp), and installing
// default SIGINT/SIGTERM handling - behind a small surface so a real app
// entry point is just:
//
//   int main(int argc, char** argv) {
//       atlas::demo::App app(argc, argv);
//       return app.run();
//   }
//
// run_ticks remains the tested, low-level tick-driving primitive this class
// is built on top of - App does not replace it, it wraps it for an actual
// application entry point. Argv parsing is intentionally minimal today
// (--ticks N only) - real runtime configuration (loading a host manifest/
// capability set at runtime rather than compile time) is a separate, larger
// concern left to a future issue, per the same review comment.
//
// Override on_tick()/stop_requested() in a derived class to customize
// per-tick behavior or the stop condition (e.g. issue #71's real
// input/render/audio host is expected to derive from this) - the default
// implementations match demo-host's original behavior exactly (a heartbeat
// log once per second, honoring SIGINT/SIGTERM to stop the loop).
//
// An encapsulated class, not a basic aggregate: it owns a real Host/Context
// pair with a genuine invariant (constructed together, in the right order,
// never copied or moved once real OS signal handlers may reference the
// process-wide stop flag it reads).
class App {
public:
    App(int argc, char** argv);
    virtual ~App() = default;

    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    // Runs the tick loop until stop_requested() or the optional --ticks
    // bound passed on argv is reached, then returns a process exit code -
    // 2 if construction failed to parse argv (print_usage() already ran),
    // 0 on an ordinary stop. Never runs a single tick if construction
    // failed to parse.
    int run();

protected:
    // Invoked once per tick, immediately *before* run_ticks/advance_tick
    // advances the tick boundary - the hook a derived class uses to turn
    // this tick's polled input into a dispatched request (issue #71) before
    // simulation resolves it, which on_tick (below, which fires after) is
    // too late for. Default: does nothing.
    virtual void pre_tick(std::uint64_t next_tick);

    // Invoked once per tick, after run_ticks/advance_tick has already
    // advanced the tick boundary - tick is the 1-based count of ticks run
    // so far this process. Default: logs a heartbeat once per second
    // (ticks_per_second). Override for real per-tick work (e.g. #71's real
    // render/audio submission).
    virtual void on_tick(std::uint64_t tick);

    // True once a stop has been requested - SIGINT/SIGTERM by default (a
    // process-wide flag, the standard C signal-handling idiom, see app.cpp).
    // Override with a different stop condition without touching signal
    // handling at all.
    [[nodiscard]] virtual bool stop_requested() const;

    [[nodiscard]] runtime::Host& host() noexcept { return host_; }
    [[nodiscard]] Context& ctx() noexcept { return ctx_; }

    // The real composed PropertyStores every demo capability's contract
    // reads/writes through ctx() - exposed directly (rather than only
    // reachable via ctx().get<T>()) so a derived class (e.g. issue #71's
    // PresentationApp) can read a store's current per-entity state (e.g.
    // movement::Position, to sync into a render::Transform) without going
    // through a capability request/property round trip that has nothing to
    // do with this demo's own gameplay capabilities.
    [[nodiscard]] DemoRuntimeHost& composition() noexcept { return composition_; }

    // Whether construction successfully parsed argv - run() consults this
    // itself, but exposed so a derived class's own constructor can bail out
    // early too (e.g. skip additional setup after a parse failure).
    [[nodiscard]] bool parsed_ok() const noexcept { return parsed_ok_; }

    // The bound passed via --ticks N, if any - exposed for the same reason
    // as parsed_ok().
    [[nodiscard]] std::optional<std::uint64_t> tick_limit() const noexcept { return tick_limit_; }

private:
    bool parsed_ok_ = true;
    std::optional<std::uint64_t> tick_limit_;
    runtime::Host host_;
    Context ctx_;
    DemoRuntimeHost composition_;
};

} // namespace atlas::demo
