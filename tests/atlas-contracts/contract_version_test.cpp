#include "atlas/contracts/contract_version.hpp"

#include <gtest/gtest.h>

namespace atlas::contracts {
namespace {

TEST(ContractVersion, EqualityComparesUnderlyingSemanticVersion) {
    constexpr ContractVersion first{.version = {1, 4, 0}};
    constexpr ContractVersion second{.version = {1, 4, 0}};
    constexpr ContractVersion different{.version = {1, 4, 1}};

    EXPECT_EQ(first, second);
    EXPECT_NE(first, different);
}

TEST(ContractVersion, ToStringRendersMajorMinorPatch) {
    constexpr ContractVersion version{.version = {1, 4, 0}};

    EXPECT_EQ(to_string(version), "1.4.0");
}

TEST(CheckContractVersion, MatchingVersionsReturnNullopt) {
    constexpr ContractVersion client{.version = {2, 0, 0}};
    constexpr ContractVersion server{.version = {2, 0, 0}};

    EXPECT_FALSE(check_contract_version(client, server).has_value());
}

TEST(CheckContractVersion, MismatchedVersionsReturnBothVersions) {
    constexpr ContractVersion client{.version = {1, 4, 0}};
    constexpr ContractVersion server{.version = {2, 0, 0}};

    const auto mismatch = check_contract_version(client, server);

    ASSERT_TRUE(mismatch.has_value());
    EXPECT_EQ(mismatch->client_version, client);
    EXPECT_EQ(mismatch->server_version, server);
}

TEST(ContractVersionMismatchTest, DescribeReportsBothVersions) {
    constexpr ContractVersionMismatch mismatch{.client_version = {.version = {1, 4, 0}},
                                               .server_version = {.version = {2, 0, 0}}};

    const auto message = describe(mismatch);

    EXPECT_NE(message.find("1.4.0"), std::string::npos);
    EXPECT_NE(message.find("2.0.0"), std::string::npos);
}

} // namespace
} // namespace atlas::contracts
