#pragma once

#include <concepts>
#include <type_traits>

namespace atlas {

// Structural baseline every hand-written or (eventually) generated contract
// struct must satisfy, regardless of which of the three declarative blocks
// (properties/requests/events, spec §13 Capability Manifest) it came from.
// Atlas's tiny-interface philosophy (§5, Tiny Interface Composability) is
// structural rather than nominal — a contract type "is" a property, a
// request, or an event because of what a manifest declared it as, not
// because its C++ shape carries a marker distinguishing the three (the §21
// worked example's Health/ApplyDamage/HealthChanged are structurally
// identical plain aggregates). PropertyContract/RequestContract/EventContract
// therefore all reduce to this one predicate today; see this library's
// README for why that is a deliberate, documented scoping decision rather
// than an oversight.
//
// - is_aggregate_v: matches the Rule-of-Zero convention generated contracts
//   follow (plain public fields, no hand-written constructor, §21) — a
//   contract describes structure, never behavior (§14, The Declarative
//   Boundary).
// - semiregular: default-constructible and copyable, the minimum a value
//   needs to be storable in per-entity property slots, request dispatch, and
//   event queues. Deliberately not `regular` — Health (§21) has no equality
//   operator, so requiring one would reject the spec's own ground-truth
//   example.
template <typename T>
concept ContractStruct = std::is_aggregate_v<T> && std::semiregular<T>;

// A property contract (§13 `properties:` block; §20 Property Composition) —
// the type stored per-entity and accessed through ctx.get<T>().
template <typename T>
concept PropertyContract = ContractStruct<T>;

// A request contract (§13 `requests:` block; §6 Terminology: Request vs.
// Internal Dispatch) — the only dispatchable contract kind, validated and
// accepted or rejected against authoritative state.
template <typename T>
concept RequestContract = ContractStruct<T>;

// An event contract (§13 `events:` block) — published via ctx.publish<T>()
// and observed by capabilities that subscribe to it.
template <typename T>
concept EventContract = ContractStruct<T>;

} // namespace atlas
