#include "atlas/replication/transport.hpp"
#include "atlas/replication/unix_socket_transport.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace atlas::replication {
namespace {

static_assert(Transport<UnixSocketTransport>);

class UnixSocketTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        directory_ = std::filesystem::temp_directory_path() /
                     std::filesystem::path("atlas-unix-socket-transport-test-" +
                                           std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
        std::filesystem::create_directories(directory_);
    }

    void TearDown() override { std::filesystem::remove_all(directory_); }

    [[nodiscard]] std::filesystem::path socket_path(const std::string& name) const {
        return directory_ / name;
    }

    // poll_received() drains a non-blocking socket, so a message sent an
    // instant ago on the same machine may not have arrived in the kernel's
    // receive buffer yet - a handful of short retries avoids a flaky test
    // without ever blocking indefinitely.
    [[nodiscard]] static std::span<const ReceivedMessage<UnixSocketTransport::Address>>
    poll_until_nonempty(UnixSocketTransport& transport) {
        for (int attempt = 0; attempt < 100; ++attempt) {
            auto received = transport.poll_received();
            if (!received.empty()) {
                return received;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return transport.poll_received();
    }

private:
    std::filesystem::path directory_;
};

TEST_F(UnixSocketTransportTest, SendAndReceiveRoundTrip) {
    UnixSocketTransport server(socket_path("server.sock"));
    UnixSocketTransport client(socket_path("client.sock"));

    const std::vector<std::byte> payload{std::byte{1}, std::byte{2}, std::byte{3}};
    EXPECT_TRUE(client.send(socket_path("server.sock"), payload));

    auto received = poll_until_nonempty(server);
    ASSERT_EQ(received.size(), 1U);
    EXPECT_EQ(received[0].sender, socket_path("client.sock"));
    EXPECT_EQ(received[0].payload, payload);
}

TEST_F(UnixSocketTransportTest, PollReceivedIsEmptyBeforeAnySend) {
    UnixSocketTransport server(socket_path("server.sock"));

    EXPECT_TRUE(server.poll_received().empty());
}

TEST_F(UnixSocketTransportTest, PollReceivedDrainsMultipleMessagesInOneBatch) {
    UnixSocketTransport server(socket_path("server.sock"));
    UnixSocketTransport client(socket_path("client.sock"));

    const std::vector<std::byte> first_payload{std::byte{1}};
    const std::vector<std::byte> second_payload{std::byte{2}};
    EXPECT_TRUE(client.send(socket_path("server.sock"), first_payload));
    EXPECT_TRUE(client.send(socket_path("server.sock"), second_payload));

    std::span<const ReceivedMessage<UnixSocketTransport::Address>> received;
    for (int attempt = 0; attempt < 100 && received.size() < 2; ++attempt) {
        received = server.poll_received();
        if (received.size() < 2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    ASSERT_EQ(received.size(), 2U);
}

TEST_F(UnixSocketTransportTest, RepeatedPollReceivedStaysEmptyOnceDrained) {
    UnixSocketTransport server(socket_path("server.sock"));
    UnixSocketTransport client(socket_path("client.sock"));

    const std::vector<std::byte> payload{std::byte{1}};
    EXPECT_TRUE(client.send(socket_path("server.sock"), payload));
    (void)poll_until_nonempty(server);

    EXPECT_TRUE(server.poll_received().empty());
}

TEST_F(UnixSocketTransportTest, SendToNonExistentDestinationFails) {
    UnixSocketTransport client(socket_path("client.sock"));

    const std::vector<std::byte> payload{std::byte{1}};
    EXPECT_FALSE(client.send(socket_path("no-such-server.sock"), payload));
}

TEST_F(UnixSocketTransportTest, ConstructorRemovesStaleSocketFileAtBindPath) {
    const auto path = socket_path("server.sock");
    { UnixSocketTransport first(path); }

    EXPECT_NO_THROW(UnixSocketTransport second(path));
}

} // namespace
} // namespace atlas::replication
