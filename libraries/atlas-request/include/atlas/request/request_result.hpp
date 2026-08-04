#pragma once

#include "atlas/contracts/contract_concepts.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace atlas {

// Outcome of a request handler (§6, Request Validation and Reconciliation):
// every on_request handler returns one of these instead of throwing or
// silently mutating an invalid request into a valid one (§21 worked
// example: `atlas::RequestResult on_request(atlas::Context&, const
// ApplyDamage&)`). Lives in the top-level `atlas` namespace rather than
// `atlas::request`, for the same reason EntityRef does (atlas-entity's
// README, Namespace note) — it is cross-library vocabulary named directly
// in every capability's generated request-handler signature.
//
// A basic aggregate (rule of zero): accept()/reject() below are the
// intended way to produce one, but nothing here protects an invariant
// across its own operations, so there is no reason to hide the fields
// behind an encapsulated class.
struct RequestResult {
    bool accepted = false;

    // Empty when accepted. reject() (below) refuses to construct a
    // rejected result with an empty reason (issue #91) — §6 frames
    // rejection as always sitting behind "a capability-defined
    // precondition," so an empty reason at that call site is a caller
    // mistake, not a meaningful rejection.
    std::string rejection_reason;

    friend bool operator==(const RequestResult&, const RequestResult&) = default;
};

// Produces an accepted result for a request handler to return (§21 worked
// example: `return atlas::accept(cmd);`). Constrained to
// atlas::RequestContract<T> so only a contract-shaped request type can
// flow through this path (§5, Tiny Interface Composability) — the request
// value itself is not inspected, only used for template argument
// deduction and the concept check, since acceptance is a decision about
// validity, not about the request's content.
template <typename T>
    requires RequestContract<T>
[[nodiscard]] constexpr RequestResult accept(const T& /*request*/) noexcept {
    return RequestResult{.accepted = true, .rejection_reason = {}};
}

// Produces a rejected result carrying a mandatory, capability-authored
// explanation (§6: "the server never silently mutates a client's request
// to make it valid" — rejection must always be explicit and always
// explained, never silent or inferred). Throws std::invalid_argument for
// an empty reason (issue #91) — an empty string is never a real
// explanation, so a call site that produces one has a bug, not a
// legitimate rejection with nothing to say.
template <typename T>
    requires RequestContract<T>
[[nodiscard]] RequestResult reject(const T& /*request*/, std::string reason) {
    if (reason.empty()) {
        throw std::invalid_argument("atlas::reject: rejection reason must not be empty");
    }
    return RequestResult{.accepted = false, .rejection_reason = std::move(reason)};
}

} // namespace atlas
