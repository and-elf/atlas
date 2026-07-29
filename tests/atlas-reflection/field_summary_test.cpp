#include "atlas/reflection/field_summary.hpp"

#include <gtest/gtest.h>

namespace atlas::reflection {
namespace {

TEST(FieldSummary, ZeroFieldsUsesPluralWording) {
    EXPECT_EQ(describe_field_count(0), "0 fields");
}

TEST(FieldSummary, OneFieldUsesSingularWording) {
    EXPECT_EQ(describe_field_count(1), "1 field");
}

TEST(FieldSummary, MultipleFieldsUsesPluralWording) {
    EXPECT_EQ(describe_field_count(2), "2 fields");
    EXPECT_EQ(describe_field_count(7), "7 fields");
}

} // namespace
} // namespace atlas::reflection
