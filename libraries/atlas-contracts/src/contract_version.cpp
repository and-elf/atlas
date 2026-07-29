#include "atlas/contracts/contract_version.hpp"

#include <format>

namespace atlas::contracts {

std::string to_string(const ContractVersion& version) {
    return std::format("{}.{}.{}",
                       version.version.major_version,
                       version.version.minor_version,
                       version.version.patch_version);
}

std::string describe(const ContractVersionMismatch& mismatch) {
    return std::format("contract version mismatch: client v{} != server v{}",
                       to_string(mismatch.client_version),
                       to_string(mismatch.server_version));
}

} // namespace atlas::contracts
