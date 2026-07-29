#include "atlas/request/request_diagnostics.hpp"
#include "atlas/request/request_result.hpp"

#include <gtest/gtest.h>

namespace atlas::request {
namespace {

TEST(RequestDiagnostics, DescribesAnAcceptedResult) {
    const RequestResult result{.accepted = true, .rejection_reason = {}};

    EXPECT_EQ(describe(result), "accepted");
}

TEST(RequestDiagnostics, DescribesARejectedResultIncludingItsReason) {
    const RequestResult result{.accepted = false, .rejection_reason = "not authoritative"};

    EXPECT_EQ(describe(result), "rejected: not authoritative");
}

} // namespace
} // namespace atlas::request
