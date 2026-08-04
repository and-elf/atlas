#include "atlas/replication/null_transport.hpp"
#include "atlas/replication/transport.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <span>

namespace atlas::replication {
namespace {

static_assert(Transport<NullTransport>);

TEST(NullTransport, SendAlwaysSucceeds) {
    NullTransport transport;
    const std::array<std::byte, 3> payload{std::byte{1}, std::byte{2}, std::byte{3}};

    const bool sent = transport.send(NullTransport::Address{}, std::span<const std::byte>{payload});

    EXPECT_TRUE(sent);
}

TEST(NullTransport, PollReceivedIsEmptyBeforeAnySend) {
    NullTransport transport;

    const std::span<const ReceivedMessage<NullTransport::Address>> received = transport.poll_received();

    EXPECT_TRUE(received.empty());
}

// NullTransport has no real socket and never loops a send back to its own
// receive queue - unlike a hypothetical loopback backend, sending must never
// make poll_received() report anything.
TEST(NullTransport, PollReceivedStaysEmptyAfterSending) {
    NullTransport transport;
    const std::array<std::byte, 1> payload{std::byte{42}};

    (void)transport.send(NullTransport::Address{}, std::span<const std::byte>{payload});
    const std::span<const ReceivedMessage<NullTransport::Address>> received = transport.poll_received();

    EXPECT_TRUE(received.empty());
}

TEST(NullTransport, RepeatedPollReceivedCallsStayEmpty) {
    NullTransport transport;

    (void)transport.poll_received();
    const std::span<const ReceivedMessage<NullTransport::Address>> second_call = transport.poll_received();

    EXPECT_TRUE(second_call.empty());
}

// Two distinct NullTransport::Address values must compare equal - it is an
// empty marker type (issue #216: "NullTransport never actually addresses
// anything, so there is nothing for a real address shape to represent
// here"), never a per-instance identity.
TEST(NullTransport, AddressValuesAlwaysCompareEqual) {
    EXPECT_EQ(NullTransport::Address{}, NullTransport::Address{});
}

} // namespace
} // namespace atlas::replication
