#pragma once

#include "atlas/core/semantic_version.hpp"

#include <optional>
#include <string>

namespace atlas::contracts {

// The exact-match contract version a host presents at connection time (§6,
// Contract Version Enforcement) — a distinct type from
// atlas::core::SemanticVersion, rather than a type alias, so it can never be
// silently interchanged with a manifest's `consumes`/`produces` semver
// *range* (§13, Manifest Versioning vs. Contract Version Enforcement): one
// is resolved by tooling at build time and admits a range; the other is
// compared for exact equality at connection time and admits none.
//
// A basic aggregate (rule of zero): no invariant here needs a constructor to
// protect.
struct ContractVersion {
    atlas::core::SemanticVersion version;

    friend constexpr auto operator<=>(const ContractVersion&, const ContractVersion&) = default;
};

// Renders "major.minor.patch" for embedding in an application-surfaced
// diagnostic (§6: e.g. "client out of date: expected vX, got vY").
[[nodiscard]] std::string to_string(const ContractVersion& version);

// The structured diagnostic §6 requires a server to expose when it refuses a
// connection over a version mismatch — both versions, never just a boolean.
struct ContractVersionMismatch {
    ContractVersion client_version;
    ContractVersion server_version;
};

// Human-readable rendering of a mismatch, for logging or direct display;
// applications remain free to format the diagnostic differently (§6: Atlas
// defines the diagnostic's structure and delivery, applications define
// presentation).
[[nodiscard]] std::string describe(const ContractVersionMismatch& mismatch);

// §6, Contract Version Enforcement: a connection is accepted only on exact
// match, never partial compatibility. Returns the mismatch diagnostic when
// the connection would be refused, or std::nullopt when it is permitted —
// never a bare bool, so a caller cannot forget to carry both versions
// through to wherever the refusal is reported.
[[nodiscard]] constexpr std::optional<ContractVersionMismatch>
check_contract_version(const ContractVersion& client_version,
                       const ContractVersion& server_version) noexcept {
    if (client_version == server_version) {
        return std::nullopt;
    }
    return ContractVersionMismatch{.client_version = client_version, .server_version = server_version};
}

} // namespace atlas::contracts
