#pragma once

#include <cstddef>
#include <vector>

namespace atlas::replication {

// One message drained from a Transport's pending receive queue (issue
// #216) - a sender address (each atlas::replication::Transport backend
// names its own Address type; see transport.hpp's own doc comment for why
// this contract deliberately does not fix one shared shape) paired with the
// raw bytes it carried. Deliberately opaque: no session id, no framing
// beyond whatever the backend itself needed to preserve this message's
// boundaries - session identity is a separate, later concern (issue #215)
// layered on top of this contract, never part of it.
template <typename AddressT> struct ReceivedMessage {
    AddressT sender;
    std::vector<std::byte> payload;

    friend bool operator==(const ReceivedMessage&, const ReceivedMessage&) = default;
};

} // namespace atlas::replication
