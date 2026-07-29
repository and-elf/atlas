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
// A basic aggregate (rule of zero): no invariant here needs a constructor to
// protect, so there's no reason to hide the fields behind one. Fields are
// named *_version rather than major/minor/patch: those names collide with
// glibc's <sys/sysmacros.h> macros on Linux, a classic portability trap this
// project deliberately avoids.
struct SemanticVersion {
    std::uint32_t major_version = 0;
    std::uint32_t minor_version = 0;
    std::uint32_t patch_version = 0;

    // Parses a strict "MAJOR.MINOR.PATCH" string (decimal, no leading '+'
    // or whitespace, no pre-release/build metadata suffix). Returns
    // std::nullopt on any deviation rather than guessing at intent.
    [[nodiscard]] static std::optional<SemanticVersion> parse(std::string_view text);

    friend constexpr auto operator<=>(const SemanticVersion&, const SemanticVersion&) = default;
};

} // namespace atlas::core
