#include "atlas/entity/entity_ref.hpp"
#include "atlas/reflection/field_visitor.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <type_traits>
#include <vector>

namespace atlas::reflection {
namespace {

// Reproduced verbatim from spec §21 (Worked Example), same precedent
// field_count_test.cpp follows — local reproductions rather than a
// cross-translation-unit shared header, since each test binary is its own TU.
struct Health {
    std::int32_t current;
    std::int32_t maximum;
};

struct ApplyDamage {
    atlas::EntityRef target;
    std::int32_t amount;
};

TEST(ForEachField, VisitsFieldsInDeclarationOrder) {
    Health health{10, 100};
    std::vector<std::int32_t> seen;
    for_each_field(health, [&seen](auto& field) { seen.push_back(field); });
    EXPECT_EQ(seen, (std::vector<std::int32_t>{10, 100}));
}

TEST(ForEachField, VisitorCanMutateFieldsThroughTheAliasedReference) {
    Health health{1, 2};
    for_each_field(health, [](auto& field) { field *= 10; });
    EXPECT_EQ(health.current, 10);
    EXPECT_EQ(health.maximum, 20);
}

TEST(ForEachField, ConstObjectYieldsConstQualifiedFieldReferences) {
    const Health health{5, 50};
    std::vector<std::int32_t> seen;
    for_each_field(health, [&seen](auto& field) {
        EXPECT_TRUE((std::is_const_v<std::remove_reference_t<decltype(field)>>));
        seen.push_back(field);
    });
    EXPECT_EQ(seen, (std::vector<std::int32_t>{5, 50}));
}

TEST(ForEachField, GenericVisitorRecoversEachFieldsRealTypeAndOrderOnAHeterogeneousContract) {
    // ApplyDamage mixes an atlas::EntityRef field with a std::int32_t field —
    // this is the "field type inspection" capability that falls out of
    // for_each_field: a generic (auto&) visitor's decltype(field) is each
    // field's genuine declared type, in declaration order, not a name (that
    // remains blocked on C++26 reflection, see this library's README).
    ApplyDamage damage{atlas::EntityRef{.index = 3, .generation = 1}, 42};
    std::vector<bool> field_is_entity_ref;
    for_each_field(damage, [&field_is_entity_ref](auto& field) {
        field_is_entity_ref.push_back(std::is_same_v<std::decay_t<decltype(field)>, atlas::EntityRef>);
    });
    ASSERT_EQ(field_is_entity_ref.size(), 2U);
    EXPECT_TRUE(field_is_entity_ref[0]);  // target (EntityRef) visited first
    EXPECT_FALSE(field_is_entity_ref[1]); // amount (int32_t) visited second
}

TEST(ForEachField, WalksTheRealEntityRefVocabularyType) {
    atlas::EntityRef ref{.index = 3, .generation = 9};
    std::vector<std::uint32_t> seen;
    for_each_field(ref, [&seen](auto& field) { seen.push_back(field); });
    EXPECT_EQ(seen, (std::vector<std::uint32_t>{3, 9}));
}

struct Empty {};

TEST(ForEachField, EmptyAggregateVisitsNoFields) {
    Empty empty{};
    int calls = 0;
    for_each_field(empty, [&calls](auto&) { ++calls; });
    EXPECT_EQ(calls, 0);
}

struct MoveOnlyAggregate {
    std::unique_ptr<int> owned;
    int value = 0;
};

TEST(ForEachField, MoveOnlyFieldsAreVisitedByReferenceNeverCopied) {
    MoveOnlyAggregate aggregate;
    aggregate.owned = std::make_unique<int>(42);
    aggregate.value = 7;
    int calls = 0;
    for_each_field(aggregate, [&calls](auto&) { ++calls; });
    EXPECT_EQ(calls, 2);
    ASSERT_NE(aggregate.owned, nullptr);
    EXPECT_EQ(*aggregate.owned, 42);
}

TEST(FieldTypes, TupleOfTypesMatchesDeclarationOrderAndDecaysCvRef) {
    static_assert(std::is_same_v<field_types_t<Health>, std::tuple<std::int32_t, std::int32_t>>);
    static_assert(std::is_same_v<field_types_t<ApplyDamage>, std::tuple<atlas::EntityRef, std::int32_t>>);
    static_assert(std::is_same_v<field_types_t<atlas::EntityRef>,
                                 std::tuple<atlas::EntityRef::IndexType, atlas::EntityRef::GenerationType>>);
    SUCCEED();
}

// --- Beyond the historical 16-field ceiling (issue #101) --------------------

// Every hand-written contract struct in this repository today still has one
// or two fields (see field_visitor.hpp's top comment); this struct exists
// purely to prove for_each_field works past the old max_supported_fields = 16
// ceiling, which is exactly what issue #101 flagged as the real limit once
// contracts grow larger.
struct TwentyFields {
    int f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19;
};

static_assert(field_count<TwentyFields>() == 20);
static_assert(FieldVisitable<TwentyFields>, "20 fields must be visitable now that the cap is raised");

TEST(ForEachField, VisitsAllTwentyFieldsInDeclarationOrderPastTheHistoricalSixteenFieldCeiling) {
    TwentyFields fields{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    std::vector<int> seen;
    for_each_field(fields, [&seen](auto& field) { seen.push_back(field); });
    EXPECT_EQ(seen, (std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19}));
}

// --- Dispatch cap boundary (max_supported_fields) ---------------------------

struct ThirtyTwoFields {
    int f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
        f22, f23, f24, f25, f26, f27, f28, f29, f30, f31;
};

static_assert(field_count<ThirtyTwoFields>() == max_supported_fields);
static_assert(FieldVisitable<ThirtyTwoFields>,
              "a type with exactly max_supported_fields direct data members must still be visitable");

TEST(ForEachField, VisitsAllFieldsExactlyAtTheSupportedCap) {
    ThirtyTwoFields all_ones{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                             1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int total = 0;
    for_each_field(all_ones, [&total](auto& field) { total += field; });
    EXPECT_EQ(total, static_cast<int>(max_supported_fields));
}

struct ThirtyThreeFields {
    int f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
        f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32;
};

static_assert(field_count<ThirtyThreeFields>() == max_supported_fields + 1);

// One field past the cap must be rejected at the FieldVisitable constraint —
// a compile-time-checkable, SFINAE-friendly failure — rather than silently
// truncating the visit or misbehaving. Expressed as static_assert(!...) so
// this is itself a real, passing part of the test suite (proof the guard
// fires), not prose asserting untested behavior.
static_assert(!FieldVisitable<ThirtyThreeFields>,
              "one field past max_supported_fields must fail the FieldVisitable constraint");

// Whether the for_each_field(T&, Visitor&&) call itself is well-formed for T,
// probed through a template rather than a bare namespace-scope
// requires-expression: substitution failure inside a call's constraint check
// is only guaranteed to be a quiet "not satisfied" (rather than a hard
// diagnostic) when it happens as part of instantiating a template — exactly
// the same reason FieldVisitable itself is a template.
template <typename T>
concept CanForEachField = requires(T& obj) { for_each_field(obj, [](auto&) {}); };

static_assert(!CanForEachField<ThirtyThreeFields>,
              "for_each_field must be unavailable, not silently incorrect, one past the cap");

TEST(FieldVisitor, OneFieldPastTheCapIsRejectedAtCompileTimeNotAtRuntime) {
    // The two static_asserts above are the actual test: if FieldVisitable
    // failed to reject ThirtyThreeFields, this translation unit would not
    // compile. This TEST exists only so the guarantee shows up in test
    // output alongside the rest of the suite.
    SUCCEED();
}

} // namespace
} // namespace atlas::reflection
