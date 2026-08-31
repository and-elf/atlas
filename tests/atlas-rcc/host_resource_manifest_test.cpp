#include "atlas/rcc/host_resource_manifest.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace atlas::rcc {
namespace {

TEST(ParseHostResourceManifest, ParsesTheHostName) {
    constexpr std::string_view yaml = "host: DedicatedServer\n";

    const HostResourceManifest manifest = parse_host_resource_manifest(yaml);

    EXPECT_EQ(manifest.host_name, "DedicatedServer");
}

TEST(ParseHostResourceManifest, RejectsNonMappingDocumentRoot) {
    constexpr std::string_view yaml = "- just\n- a\n- sequence\n";

    EXPECT_THROW({ (void)parse_host_resource_manifest(yaml); }, std::invalid_argument);
}

TEST(ParseHostResourceManifest, RejectsAMissingHostKey) {
    constexpr std::string_view yaml = "not_host: DedicatedServer\n";

    EXPECT_THROW({ (void)parse_host_resource_manifest(yaml); }, std::invalid_argument);
}

TEST(ParseHostResourceManifest, RejectsAHostKeyThatIsNotAScalar) {
    constexpr std::string_view yaml = "host: [not, a, scalar]\n";

    EXPECT_THROW({ (void)parse_host_resource_manifest(yaml); }, std::invalid_argument);
}

TEST(ParseHostResourceManifest, RejectsMalformedYaml) {
    constexpr std::string_view yaml = "host: [unterminated\n";

    EXPECT_THROW({ (void)parse_host_resource_manifest(yaml); }, std::invalid_argument);
}

} // namespace
} // namespace atlas::rcc
