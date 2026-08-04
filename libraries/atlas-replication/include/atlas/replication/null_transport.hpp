#pragma once

#include "atlas/replication/received_message.hpp"
#include "atlas/replication/transport.hpp"

#include <cstddef>
#include <span>

namespace atlas::replication {

// The always-buildable atlas::replication::Transport (issue #216): never
// actually sends or receives anything, so the mechanism up to the backend
// boundary (this contract itself) stays fully buildable and testable with
// zero third-party dependencies and no real socket at all, independent of
// whichever real backend (a future UnixTransport) a given build opts into.
//
// Address is a plain empty marker type (spec §2, Mechanism Over Meaning -
// the same "zero fields" precedent this codebase already establishes for a
// contract with nothing to carry): NullTransport never actually addresses
// anything, so there is nothing for a real address shape to represent here.
struct NullTransport {
    struct Address {
        friend constexpr bool operator==(const Address&, const Address&) noexcept = default;
    };

    // Deliberately kept as ordinary instance methods (NOLINT below silences
    // clang-tidy's own "could be static" suggestion on both) rather than
    // made `static`: every Transport is called the same way,
    // `transport.send(...)`/`transport.poll_received()`, regardless of
    // which concrete backend a caller was handed (this is exactly what
    // makes the Transport concept usable generically, transport.hpp) - a
    // real backend's own send()/poll_received() genuinely needs its own
    // instance state (an actual socket) and cannot be static, so keeping
    // these non-static too avoids two backends needing two different call
    // conventions for the same operation. Mirrors
    // atlas::physics::NullPhysicsBackend's identical raycast()/sweep()
    // rationale exactly.

    // Always true - the "local send" this backend performs is a genuine
    // no-op that cannot fail, not a simulated success. Never actually
    // enqueues destination/payload anywhere - see poll_received() below.
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    [[nodiscard]] bool send([[maybe_unused]] const Address& destination,
                            [[maybe_unused]] std::span<const std::byte> payload) noexcept {
        return true;
    }

    // Always empty - this backend has no real socket and nothing was ever
    // actually sent anywhere for it to receive back, unlike a hypothetical
    // loopback backend that might echo its own sends. Returns a
    // default-constructed (empty) span rather than a member vector: nothing
    // this backend ever does could populate one, so there is no state here
    // actually worth storing.
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    [[nodiscard]] std::span<const ReceivedMessage<Address>> poll_received() noexcept { return {}; }
};

static_assert(Transport<NullTransport>);

} // namespace atlas::replication
