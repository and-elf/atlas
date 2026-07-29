#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/request_result.hpp"

#include <cstdint>
#include <gtest/gtest.h>

namespace atlas {
namespace {

// Reproduced verbatim from spec §21 (Worked Example) — the ground-truth
// generated request contract this library's accept()/reject() must work
// against, exactly as `health.cpp`'s on_request handler does.
struct ApplyDamage {
    atlas::EntityRef target;
    std::int32_t amount;
};

TEST(RequestResult, AcceptProducesAnAcceptedResultWithNoRejectionReason) {
    const ApplyDamage cmd{.target = {}, .amount = 10};

    const RequestResult result = accept(cmd);

    EXPECT_TRUE(result.accepted);
    EXPECT_TRUE(result.rejection_reason.empty());
}

TEST(RequestResult, RejectProducesARejectedResultCarryingTheGivenReason) {
    const ApplyDamage cmd{.target = {}, .amount = 10};

    const RequestResult result = reject(cmd, "not authoritative");

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.rejection_reason, "not authoritative");
}

// §6, Request Validation and Reconciliation: "the server never silently
// mutates a client's request to make it valid" — reject() must carry
// forward exactly the caller-supplied reason, not a canned substitute.
TEST(RequestResult, RejectPreservesTheExactReasonTextVerbatim) {
    const ApplyDamage cmd{.target = {}, .amount = -5};

    const RequestResult result = reject(cmd, "target has no Health");

    EXPECT_EQ(result.rejection_reason, "target has no Health");
}

TEST(RequestResult, ResultsWithSameAcceptedFlagAndReasonCompareEqual) {
    const ApplyDamage cmd{.target = {}, .amount = 1};

    EXPECT_EQ(accept(cmd), accept(cmd));
    EXPECT_EQ(reject(cmd, "same reason"), reject(cmd, "same reason"));
    EXPECT_NE(accept(cmd), reject(cmd, "same reason"));
    EXPECT_NE(reject(cmd, "reason one"), reject(cmd, "reason two"));
}

// accept()/reject() are constrained by atlas::RequestContract<T> (§5, Tiny
// Interface Composability), the same concept atlas-contracts' own test
// suite already verifies rejects a type with a user-declared constructor
// or a non-copyable member (contract_concepts_test.cpp). Re-deriving that
// coverage here via a requires-expression hits a known GCC 13 libstdc++
// concepts diagnostic-in-immediate-context limitation (constraint failure
// deep inside std::semiregular's recursive concept chain is not SFINAE-safe),
// so this library relies on atlas-contracts' existing coverage of
// RequestContract<T> itself instead of duplicating it as a compile-fail
// check here.
static_assert(RequestContract<ApplyDamage>,
              "accept()/reject() require this to hold for the ground-truth §21 request type");

} // namespace
} // namespace atlas
