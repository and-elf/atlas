#include "atlas/diagnostics/severity.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace {

using atlas::diagnostics::Severity;
using atlas::diagnostics::to_string;

TEST(Severity, ToStringRendersEachDeclaredLevel) {
    EXPECT_EQ(to_string(Severity::Debug), "DEBUG");
    EXPECT_EQ(to_string(Severity::Info), "INFO");
    EXPECT_EQ(to_string(Severity::Warning), "WARNING");
    EXPECT_EQ(to_string(Severity::Error), "ERROR");
    EXPECT_EQ(to_string(Severity::Critical), "CRITICAL");
}

TEST(Severity, ToStringThrowsForOutOfRangeValue) {
    const auto invalid = static_cast<Severity>(255);
    EXPECT_THROW((void)to_string(invalid), std::invalid_argument);
}

} // namespace
