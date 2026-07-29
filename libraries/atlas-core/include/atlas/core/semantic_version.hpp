#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string_view>

namespace atlas::core {

// Foundational semantic-version value type. Capability manifests declare
// `contracts.consumes` version ranges resolved at build time, and hosts
// compare exact resolved contract versions at connection time (spec §6,
// §13) — both need one shared, comparable version representation.
//
// Accessors are named *_version() rather than major()/minor(): those
// names collide with glibc's <sys/sysmacros.h> macros on Linux, a classic
// portability trap this project deliberately avoids.
class SemanticVersion {
public:
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) - conventional major.minor.patch order
    constexpr SemanticVersion(std::uint32_t major_version,
                              std::uint32_t minor_version,
                              std::uint32_t patch_version) noexcept
        : major_{major_version}, minor_{minor_version}, patch_{patch_version} {}

    // Parses a strict "MAJOR.MINOR.PATCH" string (decimal, no leading '+'
    // or whitespace, no pre-release/build metadata suffix). Returns
    // std::nullopt on any deviation rather than guessing at intent.
    [[nodiscard]] static std::optional<SemanticVersion> parse(std::string_view text);

    [[nodiscard]] constexpr std::uint32_t major_version() const noexcept { return major_; }
    [[nodiscard]] constexpr std::uint32_t minor_version() const noexcept { return minor_; }
    [[nodiscard]] constexpr std::uint32_t patch_version() const noexcept { return patch_; }

    friend constexpr auto operator<=>(const SemanticVersion&, const SemanticVersion&) = default;

private:
    std::uint32_t major_;
    std::uint32_t minor_;
    std::uint32_t patch_;
};

} // namespace atlas::core
