#include "atlas/session/in_memory_session_store.hpp"

namespace atlas::session {

namespace {

// std::random_device::result_type is only guaranteed to be an unsigned
// integer type (typically 32 bits) - two draws combined into one 64-bit word
// so every bit of each SessionId word comes straight from the OS entropy
// source (see in_memory_session_store.hpp's own doc comment for why nothing
// intermediate, like a seeded PRNG engine, sits between the two).
std::uint64_t draw_secure_word(std::random_device& device) {
    const auto high_half = static_cast<std::uint64_t>(device());
    const auto low_half = static_cast<std::uint64_t>(device());
    return (high_half << 32U) | low_half;
}

} // namespace

SessionId InMemorySessionStore::create_session() {
    SessionId id{.high = draw_secure_word(device_), .low = draw_secure_word(device_)};

    // Vanishingly unlikely (128 bits of real CSPRNG output) but never
    // silently accepted if it somehow happens: neither the null sentinel nor
    // a collision with an already-active id is ever handed back to a caller.
    while (id.is_null() || active_sessions_.contains(id)) {
        id = SessionId{.high = draw_secure_word(device_), .low = draw_secure_word(device_)};
    }

    active_sessions_.insert(id);
    return id;
}

bool InMemorySessionStore::is_valid(SessionId id) const {
    return active_sessions_.contains(id);
}

void InMemorySessionStore::revoke(SessionId id) {
    // std::unordered_set::erase on a key that isn't present is a documented
    // no-op (returns 0, never throws) - revoking an already-revoked or
    // never-created session id is therefore always safe to call, matching
    // this project's "explicit, documented behavior for edge cases, never
    // silently assumed" convention (CLAUDE.md).
    active_sessions_.erase(id);
}

} // namespace atlas::session
