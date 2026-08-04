#pragma once

#include "atlas/session/session_id.hpp"

#include <concepts>

namespace atlas::session {

// The compile-time contract (spec §5: "checked like a C++ concept, never a
// runtime interface table or virtual dispatch lookup") every session store
// backend must satisfy. Mirrors atlas::render::FrameBackend/atlas::audio::
// AudioBackend's own shape exactly (libraries/atlas-render/include/atlas/
// render/frame_backend.hpp, libraries/atlas-audio/include/atlas/audio/
// audio_backend.hpp), adapted to this library's own output shape.
//
// Unlike those two, there is no Null/stub variant of this concept's real
// backend: spec §6 (Session Identity) is explicit that an in-process session
// store is not a fake awaiting a "real" implementation later — it is a fully
// valid backend for a standalone or single-shard deployment. A distributed
// backend sharing state across many server processes is a second, equally
// real implementation of this same concept, selected the same configure-time
// way atlas-render/atlas-audio/atlas-physics already select a backend — never
// a runtime factory or plugin lookup (spec §4).
//
// create_session() mints a brand new, currently-valid SessionId each call —
// never the null sentinel, and never one already active in the same store.
// is_valid() reports whether an id is currently valid: true immediately after
// create_session() returns it, false once revoke() has been called for it (or
// for any id the store never considered valid to begin with). revoke() is
// otherwise a fire-and-forget lifecycle operation with no return value to
// react to — a conforming backend documents its own exact behavior for
// revoking an already-invalid id (never/never a thrown exception per
// CLAUDE.md's "explicit, documented behavior for edge cases" convention) but
// the concept itself only constrains the signature, the same way FrameBackend
// documents submit()'s "complete, uncensored Frame" obligation as a
// contractual note rather than something the concept can check structurally.
template <typename T>
concept SessionStore = requires(T& store, SessionId id) {
    { store.create_session() } -> std::same_as<SessionId>;
    { store.is_valid(id) } -> std::same_as<bool>;
    { store.revoke(id) } -> std::same_as<void>;
};

} // namespace atlas::session
