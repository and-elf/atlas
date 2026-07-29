#include "atlas/entity/entity_ref.hpp"
#include "atlas/reflection/field_count.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>

namespace atlas::reflection {
namespace {

// Reproduced verbatim from spec §21 (Worked Example) — the generated
// health.capability.hpp contract this library's primitive must be able to
// walk generically, the same ground truth atlas-contracts's own tests use.
struct Health {
    std::int32_t current;
    std::int32_t maximum;
};

struct ApplyDamage {
    atlas::EntityRef target;
    std::int32_t amount;
};

struct HealthChanged {
    atlas::EntityRef target;
    std::int32_t new_current;
};

TEST(FieldCount, CountsBothFieldsOfAGeneratedPropertyContract) {
    EXPECT_EQ((field_count<Health>()), 2U);
}

TEST(FieldCount, CountsBothFieldsOfAGeneratedRequestContractIncludingAnEntityRef) {
    EXPECT_EQ((field_count<ApplyDamage>()), 2U);
}

TEST(FieldCount, CountsBothFieldsOfAGeneratedEventContract) {
    EXPECT_EQ((field_count<HealthChanged>()), 2U);
}

TEST(FieldCount, CountsTheRealEntityRefVocabularyTypeItself) {
    // atlas::EntityRef has two direct data members (index, generation) plus a
    // static constexpr member (null_index, not part of aggregate init) and a
    // member function — neither of the latter two is a brace-init slot, so
    // neither should be counted.
    EXPECT_EQ((field_count<atlas::EntityRef>()), 2U);
}

struct Empty {};

TEST(FieldCount, EmptyAggregateHasZeroFields) {
    EXPECT_EQ((field_count<Empty>()), 0U);
}

struct SingleField {
    int value;
};

TEST(FieldCount, SingleFieldAggregateCountsOne) {
    EXPECT_EQ((field_count<SingleField>()), 1U);
}

struct Inner {
    int x;
};

struct NestedAggregateMember {
    Inner inner;
    int y;
};

// A nested member that is itself a class-type aggregate counts as exactly one
// slot, the same as a scalar member — the placeholder used to probe arity
// (detail::AnyMember) has a valid conversion path directly to `Inner` as a
// whole (class types, unlike arrays, are legal function-template return
// types), so the compiler never falls back to brace-eliding through it.
TEST(FieldCount, NestedClassTypeAggregateMemberCountsAsOneField) {
    EXPECT_EQ((field_count<NestedAggregateMember>()), 2U);
}

struct MoveOnlyAggregate {
    std::unique_ptr<int> owned;
    int value = 0;
};

TEST(Reflectable, DefaultConstructibleAggregateWithMoveOnlyMemberStillSatisfiesReflectable) {
    // is_aggregate + default_initializable — copyability is not required here
    // (unlike atlas::ContractStruct's std::semiregular), since field_count()
    // only ever needs T{} and T{args...} to be well-formed, never a copy.
    EXPECT_TRUE((Reflectable<MoveOnlyAggregate>));
    EXPECT_EQ((field_count<MoveOnlyAggregate>()), 2U);
}

TEST(Reflectable, PlainAggregateSatisfiesReflectable) {
    EXPECT_TRUE((Reflectable<Health>));
    EXPECT_TRUE((Reflectable<atlas::EntityRef>));
}

struct HasUserDeclaredConstructor {
    explicit HasUserDeclaredConstructor(int initial) : value(initial) {}
    int value;
};

TEST(Reflectable, TypeWithUserDeclaredConstructorFailsReflectable) {
    EXPECT_FALSE((Reflectable<HasUserDeclaredConstructor>));
}

// --- Known limitations (§ README, "Known Limitations") — pinned rather than
// left as unverified prose, so a future compiler upgrade that changes this
// behavior is caught here instead of silently changing tooling output. None
// of these shapes occur in any hand-written or (eventually) generated
// contract struct in this repository today.

struct WithArrayMember {
    int values[3]; // NOLINT(*-avoid-c-arrays) — exercising a known limitation
    int total;
};

// A raw C-array member has no single-object conversion AnyMember can target
// (a function template cannot be instantiated to return an array type), so
// the compiler brace-elides through it and consumes one initializer per
// *array element* instead of treating the whole array as one field. The
// technique reports the flattened leaf count (3 + 1 = 4), not the direct
// member count (2) a human would expect.
TEST(FieldCount, KnownLimitationArrayMembersAreFlattenedRatherThanCountedAsOneField) {
    EXPECT_EQ((field_count<WithArrayMember>()), 4U);
}

struct BaseFields {
    int a;
    int b;
};

struct DerivedFields : BaseFields {
    int c;
};

// A base class subobject *is* a class type, so AnyMember converts to it
// directly as a single unit (same mechanism as NestedAggregateMember above) —
// but that makes the base class read as exactly one field regardless of how
// many members it itself has, not zero (excluded) and not fully flattened.
// DerivedFields{X, Y} binds X to the whole BaseFields base and Y to `c`;
// a third initializer has no subobject left to bind to.
TEST(FieldCount, KnownLimitationBaseClassCountsAsOneOpaqueFieldRegardlessOfItsOwnMemberCount) {
    EXPECT_EQ((field_count<DerivedFields>()), 2U);
}

struct WithBitfieldMember {
    int flag : 4; // NOLINT(*-avoid-magic-numbers)
    int total;
};

// Bit-fields are frequently cited as a failure mode for this family of
// techniques (they matter for structured-binding-based reflection, which
// cannot bind a reference to a bit-field). This brace-counting technique
// never takes a bit-field's address or binds a reference to it, so it counts
// bit-field members correctly — verified here rather than assumed.
TEST(FieldCount, BitfieldMembersAreCountedCorrectly) {
    EXPECT_EQ((field_count<WithBitfieldMember>()), 2U);
}

struct WithUninitializedReferenceMember {
    int& ref; // NOLINT(*-avoid-const-or-ref-data-members) — exercising a known limitation
    int value;
};

// A reference member with no default member initializer makes the aggregate
// not default-constructible (there is no such thing as an unbound
// reference), so it fails Reflectable's default_initializable requirement
// before field_count() would even be reachable — the concept constraint
// itself is the guard here, not special-cased detection inside field_count().
TEST(Reflectable, ReferenceMemberWithoutADefaultInitializerFailsReflectable) {
    EXPECT_FALSE((Reflectable<WithUninitializedReferenceMember>));
}

} // namespace
} // namespace atlas::reflection
