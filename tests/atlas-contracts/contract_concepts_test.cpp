#include "atlas/contracts/contract_concepts.hpp"
#include "atlas/entity/entity_ref.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>

// Generated at build time by atlas-cgen from tests/fixtures/health.capability.yaml
// (see tests/atlas-contracts/CMakeLists.txt) - the real generator output this
// library's concepts must accept, not a hand-copied stand-in that could
// silently drift from what atlas-cgen actually emits (spec §21 worked
// example).
#include "health.capability.hpp"

namespace atlas {
namespace {

using atlas::health::ApplyDamage;
using atlas::health::Health;
using atlas::health::HealthChanged;

TEST(ContractConcepts, GeneratedPropertyContractSatisfiesPropertyContract) {
    EXPECT_TRUE((PropertyContract<Health>));
}

TEST(ContractConcepts, GeneratedRequestContractSatisfiesRequestContract) {
    EXPECT_TRUE((RequestContract<ApplyDamage>));
}

TEST(ContractConcepts, GeneratedEventContractSatisfiesEventContract) {
    EXPECT_TRUE((EventContract<HealthChanged>));
}

// Atlas's tiny-interface philosophy (§5) is structural, not nominal: nothing
// about Health's shape marks it specifically as a "property" versus a
// "request" or "event" — the manifest block it came from is what assigns
// meaning, not the type itself. All three concepts therefore accept any
// plain contract-shaped aggregate.
TEST(ContractConcepts, AllThreeConceptsAcceptAnyPlainContractStruct) {
    EXPECT_TRUE((PropertyContract<ApplyDamage>));
    EXPECT_TRUE((RequestContract<Health>));
    EXPECT_TRUE((EventContract<Health>));
}

struct HasUserDeclaredConstructor {
    explicit HasUserDeclaredConstructor(std::int32_t initial) : value(initial) {}
    std::int32_t value;
};

TEST(ContractConcepts, TypeWithUserDeclaredConstructorFailsAllThreeConcepts) {
    EXPECT_FALSE((PropertyContract<HasUserDeclaredConstructor>));
    EXPECT_FALSE((RequestContract<HasUserDeclaredConstructor>));
    EXPECT_FALSE((EventContract<HasUserDeclaredConstructor>));
}

struct NotCopyable {
    std::unique_ptr<int> owned;
};

TEST(ContractConcepts, NonCopyableTypeFailsAllThreeConcepts) {
    EXPECT_FALSE((PropertyContract<NotCopyable>));
    EXPECT_FALSE((RequestContract<NotCopyable>));
    EXPECT_FALSE((EventContract<NotCopyable>));
}

} // namespace
} // namespace atlas
