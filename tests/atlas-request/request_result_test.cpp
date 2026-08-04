#include "atlas/entity/entity_ref.hpp"
#include "atlas/request/request_result.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <string>

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

// §6, Request Trust and Permission: rejection always sits behind "a
// capability-defined precondition" - an empty reason is a caller mistake at
// the call site, not a meaningful rejection, so reject() refuses it outright
// (issue #91) rather than silently accepting a string with nothing in it.
TEST(RequestResult, RejectThrowsOnAnEmptyReason) {
    const ApplyDamage cmd{.target = {}, .amount = 10};

    EXPECT_THROW((void)reject(cmd, ""), std::invalid_argument);
}

TEST(RequestResult, ResultsWithSameAcceptedFlagAndReasonCompareEqual) {
    const ApplyDamage cmd{.target = {}, .amount = 1};

    EXPECT_EQ(accept(cmd), accept(cmd));
    EXPECT_EQ(reject(cmd, "same reason"), reject(cmd, "same reason"));
    EXPECT_NE(accept(cmd), reject(cmd, "same reason"));
    EXPECT_NE(reject(cmd, "reason one"), reject(cmd, "reason two"));
}

static_assert(RequestContract<ApplyDamage>,
              "accept()/reject() require this to hold for the ground-truth §21 request type");

// accept()/reject() are constrained by atlas::RequestContract<T> (§5, Tiny
// Interface Composability), the same concept atlas-contracts' own test suite
// already verifies rejects a type with a user-declared constructor or a
// non-copyable member (contract_concepts_test.cpp). Re-deriving that as a
// compile-fail check written directly against accept()/reject() (e.g.
// `static_assert(!requires(const Bad& c) { atlas::accept(c); });`) still
// hard-errors instead of evaluating to false - confirmed against both
// toolchains this project's CI builds with (GCC 13.3.0 and Clang 18 +
// libstdc++, re-verified for issue #93) - because the constraint failure
// happens deep inside std::semiregular's recursive concept chain, which is
// not SFINAE-safe there. Naming the requires-expression as its own concept
// first, and negating *that* concept in the static_assert below, sidesteps
// the limitation on both compilers and gives this library its own
// call-site-level compile-fail coverage instead of relying solely on
// atlas-contracts' suite proving RequestContract<T> itself rejects these
// types.
template <typename T>
concept CanAcceptRequest = requires(const T& request) { atlas::accept(request); };

template <typename T>
concept CanRejectRequest = requires(const T& request, std::string reason) { atlas::reject(request, reason); };

static_assert(CanAcceptRequest<ApplyDamage>, "accept() must compile for the ground-truth §21 request type");
static_assert(CanRejectRequest<ApplyDamage>, "reject() must compile for the ground-truth §21 request type");

// Not an aggregate (user-declared constructor), so it fails RequestContract<T>
// - accept()/reject() must independently refuse to compile against it too,
// not just the concept in isolation.
struct HasUserDeclaredConstructor {
    explicit HasUserDeclaredConstructor(std::int32_t initial) : value(initial) {}
    std::int32_t value;
};

static_assert(!CanAcceptRequest<HasUserDeclaredConstructor>,
              "accept() must reject a type with a user-declared constructor");
static_assert(!CanRejectRequest<HasUserDeclaredConstructor>,
              "reject() must reject a type with a user-declared constructor");

// Not std::semiregular (non-copyable member), so it fails RequestContract<T>
// - accept()/reject() must independently refuse to compile against it too.
struct NotCopyable {
    std::unique_ptr<int> owned;
};

static_assert(!CanAcceptRequest<NotCopyable>, "accept() must reject a non-copyable type");
static_assert(!CanRejectRequest<NotCopyable>, "reject() must reject a non-copyable type");

} // namespace
} // namespace atlas
