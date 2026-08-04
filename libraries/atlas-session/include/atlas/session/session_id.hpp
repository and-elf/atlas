#pragma once

#include <cstdint>
#include <functional>

namespace atlas {

// Fundamental runtime vocabulary type, referenced directly as `atlas::SessionId`
// by capabilities declaring a session-carrying request field (spec §6, Session
// Identity) — hence the top-level `atlas` namespace rather than `atlas::session`,
// even though this header is owned by and physically lives under the
// atlas-session library. Mirrors atlas::EntityRef's own header (libraries/
// atlas-entity/include/atlas/entity/entity_ref.hpp), this repository's
// established precedent for this exact namespace placement.
//
// A basic aggregate (Rule of Zero): no invariant needs protecting here beyond
// the default "no session" sentinel value, so there's no reason to hide the
// fields behind a constructor. atlas::session::InMemorySessionStore (which
// does protect a real invariant — which ids are currently valid) stays a
// proper encapsulated class.
//
// Security note (spec §6, Session Identity — read in full before changing
// this type): the two 64-bit words together give 128 bits of representation,
// wide enough that a SessionId must never be produced by anything
// predictable. In particular, never generate one from atlas::core::Random —
// that type is the deterministic *simulation* randomness source (spec §4:
// seeded, reproducible, replayable per host); reusing it here would make
// session identity guessable to anyone who could ever infer or replay a
// host's simulation seed, defeating the entire purpose of an unguessable
// identity. This type only carries the bits — atlas::session::
// InMemorySessionStore is what actually draws them, from a real CSPRNG /
// OS entropy source. The all-zero value (the default) is reserved as the
// "no session" sentinel and is never a value a real SessionStore hands out.
struct SessionId {
    std::uint64_t high = 0;
    std::uint64_t low = 0;

    [[nodiscard]] constexpr bool is_null() const noexcept { return high == 0 && low == 0; }

    friend constexpr bool operator==(const SessionId&, const SessionId&) noexcept = default;
};

} // namespace atlas

template <> struct std::hash<atlas::SessionId> {
    // Combines both words (the same shape atlas::EntityRef's own std::hash
    // specialization uses): unlike atlas::ResourceId's hash, which reuses its
    // single already-random field directly, SessionId doesn't have a single
    // designated "random enough" field to special-case — treating both words
    // uniformly (XOR-with-a-shifted second word) is simplest and correct
    // either way, since both words are already CSPRNG output.
    std::size_t operator()(const atlas::SessionId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.high) ^ (std::hash<std::uint64_t>{}(id.low) << 1U);
    }
};
