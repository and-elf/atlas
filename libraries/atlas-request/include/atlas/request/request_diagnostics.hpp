#pragma once

#include "atlas/request/request_result.hpp"

#include <string>

namespace atlas::request {

// Human-readable rendering of a RequestResult — e.g. for the uniform
// runtime failure channel (§6, Runtime Failure Reporting) once
// atlas-runtime exists to actually publish request rejections onto it.
// Library-internal mechanism with no cross-library vocabulary role, hence
// atlas::request rather than top-level atlas, unlike RequestResult itself
// (see that header's namespace note).
[[nodiscard]] std::string describe(const RequestResult& result);

} // namespace atlas::request
