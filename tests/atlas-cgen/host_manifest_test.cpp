#include "atlas/cgen/host_manifest.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace atlas::cgen {
namespace {

// Spec §14's own shape, verbatim: `host:` + `composes:`.
constexpr std::string_view gameplay_client_manifest = R"(
host: GameplayClient
composes:
  - entity
  - health
  - health_ui_bridge
)";

TEST(ParseHostManifest, ParsesTheHostNameAndComposesList) {
    const HostManifest manifest = parse_host_manifest(gameplay_client_manifest);

    EXPECT_EQ(manifest.host_name, "GameplayClient");
    ASSERT_EQ(manifest.composes.size(), 3U);
    EXPECT_EQ(manifest.composes[0], "entity");
    EXPECT_EQ(manifest.composes[1], "health");
    EXPECT_EQ(manifest.composes[2], "health_ui_bridge");
}

TEST(ParseHostManifest, TreatsAMissingComposesBlockAsEmpty) {
    constexpr std::string_view yaml = "host: DedicatedServer\n";

    const HostManifest manifest = parse_host_manifest(yaml);

    EXPECT_EQ(manifest.host_name, "DedicatedServer");
    EXPECT_TRUE(manifest.composes.empty());
}

TEST(ParseHostManifest, RejectsNonMappingDocumentRoot) {
    constexpr std::string_view yaml = "- just\n- a\n- sequence\n";

    EXPECT_THROW({ (void)parse_host_manifest(yaml); }, std::invalid_argument);
}

TEST(ParseHostManifest, RejectsAMissingHostKey) {
    constexpr std::string_view yaml = "composes: [entity]\n";

    EXPECT_THROW({ (void)parse_host_manifest(yaml); }, std::invalid_argument);
}

TEST(ParseHostManifest, RejectsAHostKeyThatIsNotAScalar) {
    constexpr std::string_view yaml = "host: [not, a, scalar]\ncomposes: [entity]\n";

    EXPECT_THROW({ (void)parse_host_manifest(yaml); }, std::invalid_argument);
}

TEST(ParseHostManifest, RejectsAComposesBlockThatIsNotASequence) {
    constexpr std::string_view yaml = "host: GameplayClient\ncomposes: entity\n";

    EXPECT_THROW({ (void)parse_host_manifest(yaml); }, std::invalid_argument);
}

TEST(ParseHostManifest, RejectsMalformedYaml) {
    constexpr std::string_view yaml = "host: [unterminated\n";

    EXPECT_THROW({ (void)parse_host_manifest(yaml); }, std::invalid_argument);
}

} // namespace
} // namespace atlas::cgen
