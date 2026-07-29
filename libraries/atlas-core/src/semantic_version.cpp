#include "atlas/core/semantic_version.hpp"

#include <charconv>

namespace atlas::core {

namespace {

std::optional<std::uint32_t> parse_component(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    std::uint32_t value{};
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }
    return value;
}

} // namespace

std::optional<SemanticVersion> SemanticVersion::parse(std::string_view text) {
    const auto first_dot = text.find('.');
    if (first_dot == std::string_view::npos) {
        return std::nullopt;
    }

    const auto second_dot = text.find('.', first_dot + 1);
    if (second_dot == std::string_view::npos) {
        return std::nullopt;
    }

    const auto major_version = parse_component(text.substr(0, first_dot));
    const auto minor_version = parse_component(text.substr(first_dot + 1, second_dot - first_dot - 1));
    const auto patch_version = parse_component(text.substr(second_dot + 1));

    if (!major_version || !minor_version || !patch_version) {
        return std::nullopt;
    }

    return SemanticVersion{*major_version, *minor_version, *patch_version};
}

} // namespace atlas::core
