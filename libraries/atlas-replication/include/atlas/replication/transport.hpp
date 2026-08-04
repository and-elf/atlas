#pragma once

#include "atlas/replication/received_message.hpp"

#include <concepts>
#include <cstddef>
#include <span>

namespace atlas::replication {

// The compile-time contract (spec §5: "checked like a C++ concept, never a
// runtime interface table or virtual dispatch lookup") every client<->server
// transport backend - real or null - must satisfy, mirroring
// atlas::physics::PhysicsBackend / atlas::audio::AudioBackend /
// atlas::render::FrameBackend exactly. Which concrete type actually compiles
// into a given build is a configure-time CMake choice, never a runtime
// factory or plugin lookup (spec §4).
//
// This is issue #216's own minimal slice - phase 1 of #215's umbrella:
//
// - Each backend names its own nested Address type (T::Address below)
//   rather than this contract fixing one shared shape. A Unix domain
//   socket's address is a filesystem path; a future UDP backend's is an
//   IP:port - forcing one shape now would either over-fit the first real
//   backend or under-specify the next one. The only requirement is
//   std::regular (default-constructible, copyable, equality-comparable), so
//   a session-identity map (issue #215's later phase) can key on it.
// - send(destination, payload) is fire-and-forget: it reports whether the
//   local send call itself succeeded (a real, synchronously-knowable local
//   fact - an invalid address, a full local buffer), never whether the
//   message actually arrived, which no transport this contract will ever
//   have can know for certain (see below).
// - poll_received() drains the *entire* pending queue in one call and
//   returns it as a span, non-blocking - mirroring
//   atlas::physics::PhysicsBackend's already-established
//   poll_contact_events() precedent (caller polls once per tick, never a
//   callback registered across an unrelated API boundary). This is what
//   lets a single per-tick call site scale to however many messages
//   actually arrived between ticks, not just one.
//
// Deliberately absent from this contract, and why:
//
// - No connect()/bind()/disconnect(). Establishing a backend's local
//   endpoint is exactly as backend-specific as its Address type - a Unix
//   socket needs a filesystem path to bind, a future UDP backend needs a
//   port - so it is each backend's own constructor's job, the same way
//   JoltPhysicsBackend's world/device init happens in its constructor, not
//   as a shared PhysicsBackend::initialize() method. Lifecycle teardown is
//   RAII (the destructor), matching every other backend contract in this
//   codebase - none of them has an explicit shutdown method either.
// - No delivery guarantee beyond "the local send call worked." This
//   contract is deliberately as weak as the weakest realistic backend can
//   honestly promise (unordered, potentially lossy, at-most-once) - #72's
//   entire motivation for this arc is avoiding TCP's head-of-line blocking,
//   which only matters if loss/reordering are real possibilities this
//   contract admits rather than papers over. A backend that happens to
//   behave better (e.g. a future UnixTransport, never realistically
//   dropping anything on localhost) does not get to promise that at the
//   contract level - nothing above this layer may depend on a guarantee
//   that would silently break the moment a real network backend replaces
//   it.
// - No session id, no framing beyond message boundaries. Session identity
//   is issue #215's later, separate phase, built on top of this contract's
//   opaque payload bytes, never part of it.
template <typename T>
concept Transport =
    requires { typename T::Address; } && std::regular<typename T::Address> &&
    requires(T& transport, const typename T::Address& destination, std::span<const std::byte> payload) {
        { transport.send(destination, payload) } -> std::same_as<bool>;
        { transport.poll_received() } -> std::same_as<std::span<const ReceivedMessage<typename T::Address>>>;
    };

} // namespace atlas::replication
