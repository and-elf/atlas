#pragma once

#include "atlas/replication/received_message.hpp"
#include "atlas/replication/transport.hpp"

#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

namespace atlas::replication {

// The first real (non-null) atlas::replication::Transport (issue #218, phase
// 2 of #215's client<->server umbrella): a genuine AF_UNIX/SOCK_DGRAM Unix
// domain socket, one server-side socket demultiplexing many client peer
// addresses - not one socket per client, per #215's own scaling decision.
// Satisfies the same Transport concept NullTransport does (transport.hpp) -
// a caller composing replication never branches on which concrete backend
// it was handed.
//
// Address = std::filesystem::path (issue #218's own decision, matching
// transport.hpp's own doc comment): a Unix domain socket's address genuinely
// is the filesystem path its peer bound to (AF_UNIX's own
// sockaddr_un::sun_path) - nothing invented on top of it.
//
// Message-oriented, not stream-oriented (#215's own "SOCK_DGRAM, not
// SOCK_STREAM" decision): each send() is one recvfrom() on the other end,
// message boundaries preserved natively by the kernel, matching the eventual
// UDP backend's own packet semantics - no length-prefix framing invented
// here.
//
// POSIX only. AF_UNIX exists on Windows too (afunix.h, Winsock2, Windows
// 10+) but that path is unverified in this round - this repo's own sandbox
// is Linux-only, and this codebase's own established precedent
// (atlas-render's README) is to flag an unverified platform path honestly
// rather than claim it works. The .cpp file includes POSIX-only headers
// unconditionally rather than behind a platform #if - a Windows build of
// this backend is real, undesigned follow-up work (issue #218's own
// "Explicitly out of scope"), not attempted speculatively here.
//
// Establishes its local endpoint in its own constructor - creates the
// socket, unlinks any stale socket file left at bind_path by a previous,
// uncleanly-terminated process, binds, sets non-blocking - rather than a
// separate connect()/bind() method: Transport's own contract deliberately
// has neither (transport.hpp's doc comment), mirroring JoltPhysicsBackend's
// own "establish real resources in the constructor" precedent.
//
// Non-copyable, non-movable: owns a live OS socket file descriptor bound to
// a real filesystem path, with a genuine invariant to protect (the bind path
// must be unlinked by the same instance that created it, exactly once, in
// its own destructor) - the same Rule-of-Zero exception CLAUDE.md carves out
// for JoltPhysicsBackend/Sdl3FrameBackend.
class UnixSocketTransport {
public:
    using Address = std::filesystem::path;

    // Throws std::invalid_argument if bind_path's string representation
    // can't fit in a sockaddr_un::sun_path (platform-defined, ~104-108
    // bytes), or std::runtime_error if socket()/bind() itself fails (e.g. an
    // existing directory at that path, or insufficient permissions).
    explicit UnixSocketTransport(std::filesystem::path bind_path);
    ~UnixSocketTransport();

    UnixSocketTransport(const UnixSocketTransport&) = delete;
    UnixSocketTransport& operator=(const UnixSocketTransport&) = delete;
    UnixSocketTransport(UnixSocketTransport&&) = delete;
    UnixSocketTransport& operator=(UnixSocketTransport&&) = delete;

    // Reports only whether the local sendto() call itself succeeded - never
    // whether the message actually arrived, exactly as Transport's own
    // contract promises (transport.hpp). Fails (returns false) if
    // destination names no socket currently bound to that path, or the
    // destination path can't fit in a sockaddr_un, among other local
    // failures - never throws.
    [[nodiscard]] bool send(const Address& destination, std::span<const std::byte> payload) const;

    // Drains the entire pending queue via a non-blocking recvfrom() loop,
    // mirroring atlas::physics::PhysicsBackend::poll_contact_events()'s own
    // batch-per-call precedent. The returned span aliases this instance's
    // own internal buffer, valid until the next poll_received() call -
    // overwritten, not appended to, on every call, the same "this round's
    // batch only" convention poll_contact_events() itself uses.
    [[nodiscard]] std::span<const ReceivedMessage<Address>> poll_received();

private:
    std::filesystem::path bind_path_;
    int socket_fd_;
    std::vector<ReceivedMessage<Address>> received_;
};

static_assert(Transport<UnixSocketTransport>);

} // namespace atlas::replication
