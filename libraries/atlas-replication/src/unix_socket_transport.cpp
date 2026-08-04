#include "atlas/replication/unix_socket_transport.hpp"

// POSIX-only (this file's own header doc comment) - deliberately included
// unconditionally rather than behind a platform #if, since this backend is
// only ever compiled in when ATLAS_REPLICATION_TRANSPORT=UNIX
// (libraries/atlas-replication/CMakeLists.txt).
#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

namespace atlas::replication {
namespace {

// sockaddr_un::sun_path is a fixed-size char array (platform-defined, ~104
// bytes on macOS/BSD, 108 on Linux - verified against both platforms'
// <sys/un.h> rather than assumed) - never large enough to size dynamically,
// so a path that doesn't fit (with room for the null terminator sendto()/
// bind() themselves rely on) is a real, reportable failure rather than
// something to truncate silently.
[[nodiscard]] sockaddr_un make_sockaddr_un(const std::filesystem::path& path) {
    const std::string path_string = path.string();
    sockaddr_un address{};
    if (path_string.size() >= sizeof(address.sun_path)) {
        throw std::invalid_argument("atlas::replication::UnixSocketTransport: bind path '" + path_string +
                                    "' does not fit in sockaddr_un::sun_path");
    }

    address.sun_family = AF_UNIX;
    // address was zero-initialized above (sockaddr_un address{}), so
    // sun_path is already fully null-padded - this copy plus the explicit
    // terminator below just makes the null-termination invariant obvious at
    // this call site rather than relying on the zero-init alone.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay) - memcpy needs a raw pointer.
    std::memcpy(address.sun_path, path_string.c_str(), path_string.size());
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index) - bounded by the check above.
    address.sun_path[path_string.size()] = '\0';
    return address;
}

// Establishes this backend's local endpoint (issue #218): validates
// bind_path fits a sockaddr_un, creates the socket, removes any stale
// socket file left at bind_path by a previous, uncleanly-terminated
// process, binds, and sets non-blocking. Pulled out of the constructor body
// into its own function so socket_fd_ can be a genuine member-initializer
// (cppcoreguidelines-prefer-member-initializer) rather than assigned after
// construction - every failure path here closes whatever it opened before
// throwing, so a failed construction never leaks the fd.
[[nodiscard]] int bind_unix_datagram_socket(const std::filesystem::path& bind_path) {
    const sockaddr_un address = make_sockaddr_un(bind_path);

    const int socket_fd = ::socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        throw std::runtime_error("atlas::replication::UnixSocketTransport: socket() failed: " +
                                 std::string(std::strerror(errno)));
    }

    // Remove any stale socket file a previous, uncleanly-terminated process
    // left at this path - ENOENT (nothing there to remove) is the expected,
    // common case, not a failure; any other errno is still not fatal here,
    // since the immediately-following bind() call is the real authority on
    // whether this path is usable.
    ::unlink(bind_path.c_str());

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - bind() needs a const sockaddr*.
    const auto* raw_address = reinterpret_cast<const sockaddr*>(&address);
    if (::bind(socket_fd, raw_address, sizeof(address)) < 0) {
        const std::string bind_error = std::strerror(errno);
        ::close(socket_fd);
        throw std::runtime_error("atlas::replication::UnixSocketTransport: bind('" + bind_path.string() +
                                 "') failed: " + bind_error);
    }

    // poll_received() must never block waiting for a message that hasn't
    // arrived yet - Transport's own contract (transport.hpp) requires
    // poll_received() to be non-blocking.
    const int flags = ::fcntl(socket_fd, F_GETFL, 0);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) - fcntl() is POSIX's own vararg API.
    ::fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);

    return socket_fd;
}

} // namespace

UnixSocketTransport::UnixSocketTransport(std::filesystem::path bind_path)
    : bind_path_(std::move(bind_path)), socket_fd_(bind_unix_datagram_socket(bind_path_)) {}

UnixSocketTransport::~UnixSocketTransport() {
    ::close(socket_fd_);
    ::unlink(bind_path_.c_str());
}

bool UnixSocketTransport::send(const Address& destination, std::span<const std::byte> payload) const {
    sockaddr_un address{};
    try {
        address = make_sockaddr_un(destination);
    } catch (const std::invalid_argument&) {
        return false;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - sendto() needs a const sockaddr*.
    const auto* raw_address = reinterpret_cast<const sockaddr*>(&address);
    const ssize_t sent =
        ::sendto(socket_fd_, payload.data(), payload.size(), 0, raw_address, sizeof(address));
    return sent >= 0;
}

std::span<const ReceivedMessage<UnixSocketTransport::Address>> UnixSocketTransport::poll_received() {
    received_.clear();

    // 65536 bytes comfortably exceeds any replication payload this codebase
    // produces today (issue #218's own scope is proving the mechanism, not
    // sizing it for a real workload) - a real per-message size budget is
    // undesigned follow-up work once a genuine caller needs one.
    std::array<std::byte, 65536> buffer{};

    while (true) {
        sockaddr_un sender_address{};
        socklen_t sender_length = sizeof(sender_address);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - recvfrom() needs a sockaddr*.
        auto* raw_sender_address = reinterpret_cast<sockaddr*>(&sender_address);
        const ssize_t received_bytes =
            ::recvfrom(socket_fd_, buffer.data(), buffer.size(), 0, raw_sender_address, &sender_length);
        if (received_bytes < 0) {
            // EAGAIN/EWOULDBLOCK: the pending queue is fully drained - the
            // expected, non-error way this loop ends every time. Any other
            // errno is a real local failure with nothing further this
            // backend can do about it; either way, stop draining.
            break;
        }

        // sender_address was zero-initialized above, so sun_path is already
        // null-padded past whatever the kernel actually wrote into it -
        // constructing a std::string from the C-string is safe without
        // separately tracking sender_length's own path-length contribution.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay) - std::string needs a C-string.
        const Address sender(std::string(sender_address.sun_path));
        received_.push_back(ReceivedMessage<Address>{
            .sender = sender,
            .payload = std::vector<std::byte>(buffer.begin(), buffer.begin() + received_bytes)});
    }

    return received_;
}

} // namespace atlas::replication
