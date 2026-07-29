#include "atlas/request/request_diagnostics.hpp"

namespace atlas::request {

std::string describe(const RequestResult& result) {
    if (result.accepted) {
        return "accepted";
    }
    return "rejected: " + result.rejection_reason;
}

} // namespace atlas::request
