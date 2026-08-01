#include "atlas/diagnostics/record.hpp"

#include <gtest/gtest.h>

namespace {

using atlas::diagnostics::DetailField;
using atlas::diagnostics::Record;
using atlas::diagnostics::Severity;

TEST(Record, DefaultsToInfoSeverityAndEmptyFields) {
    const Record record;

    EXPECT_EQ(record.severity, Severity::Info);
    EXPECT_TRUE(record.system.empty());
    EXPECT_TRUE(record.message.empty());
    EXPECT_TRUE(record.details.empty());
}

TEST(Record, StoresSeveritySystemMessageAndOrderedDetails) {
    const Record record{
        .severity = Severity::Error,
        .system = "resource",
        .message = "failed to resolve resource",
        .details = {{.key = "resource_id", .value = "42"}, {.key = "reason", .value = "not_found"}}};

    EXPECT_EQ(record.severity, Severity::Error);
    EXPECT_EQ(record.system, "resource");
    EXPECT_EQ(record.message, "failed to resolve resource");
    ASSERT_EQ(record.details.size(), 2U);
    EXPECT_EQ(record.details[0], (DetailField{.key = "resource_id", .value = "42"}));
    EXPECT_EQ(record.details[1], (DetailField{.key = "reason", .value = "not_found"}));
}

TEST(Record, DetailFieldOrderIsPreservedNotReordered) {
    const Record record{.details = {{.key = "b", .value = "2"}, {.key = "a", .value = "1"}}};

    ASSERT_EQ(record.details.size(), 2U);
    EXPECT_EQ(record.details[0].key, "b");
    EXPECT_EQ(record.details[1].key, "a");
}

TEST(Record, EqualityComparesAllFields) {
    const Record first{.severity = Severity::Warning, .system = "request", .message = "rejected"};
    const Record& second = first;
    Record third = first;
    third.message = "different";

    EXPECT_EQ(first, second);
    EXPECT_NE(first, third);
}

} // namespace
