#pragma once

#include "atlas/session/session_id.hpp"
#include "atlas/session/session_store.hpp"

#include <random>
#include <unordered_set>

namespace atlas::session {

// The real, first SessionStore backend (spec §6, Session Identity: "not a
// fake awaiting a real implementation... it is a fully valid backend for a
// standalone or single-shard deployment"). Tracks currently-valid SessionIds
// entirely in-process; a distributed backend sharing state across many
// server processes is a separate, later implementation of the same
// SessionStore concept (explicitly out of scope for this round — issue #222).
//
// An encapsulated class, not a rule-of-zero aggregate (CLAUDE.md, Rule of
// Zero): the invariant this protects is "every id reported valid by
// is_valid() was actually minted by this store's own create_session() and
// hasn't been revoked since" — real state to protect, the same category of
// invariant atlas::entity::EntityRegistry protects for its own slot/free-list
// consistency.
//
// Security (spec §6, Session Identity — see session_id.hpp's own doc comment
// for the full rationale): every SessionId this store hands out is drawn
// directly from std::random_device, never atlas::core::Random. Drawing
// directly from std::random_device for every word — rather than seeding a
// std::mt19937_64-class engine from it once and drawing many ids from that
// engine's own output — deliberately avoids a different, more subtle
// weakness: std::mt19937's internal state is fully reconstructable from a
// long enough run of consecutive outputs, which would make every *future*
// SessionId predictable to anyone who has observed enough *past* ones from
// the same engine, even though the engine's initial seed was itself
// unguessable. Calling std::random_device directly for each 64-bit word has
// no such state to reconstruct — each word is independent OS-entropy output.
// This does cost a fresh call to the OS's entropy source per id rather than
// per process, but create_session() is called once per connection, not once
// per tick, so that cost is immaterial next to the security property it buys.
class InMemorySessionStore {
public:
    [[nodiscard]] SessionId create_session();
    [[nodiscard]] bool is_valid(SessionId id) const;
    void revoke(SessionId id);

private:
    std::random_device device_;
    std::unordered_set<SessionId> active_sessions_;
};

static_assert(SessionStore<InMemorySessionStore>);

} // namespace atlas::session
