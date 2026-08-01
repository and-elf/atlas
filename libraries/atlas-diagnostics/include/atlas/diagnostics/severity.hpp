#pragma once

#include <cstdint>
#include <string>

namespace atlas::diagnostics {

// Severity level of a diagnostic Record (see record.hpp). Five levels span
// the range spec §6's Runtime Failure Reporting examples cover: an
// expected, capability-defined request rejection is a materially
// different kind of occurrence than a resource failing to resolve, or a
// host disconnecting unexpectedly - collapsing all of them into a single
// "failure" severity would throw away exactly the distinction a future
// filtering/replay tool (this library exists to make possible, see the
// library README) needs between "routine, expected outcome worth
// recording" and "something is actually broken." Declared in ascending
// order of severity; to_string()'s implementation relies on that order
// matching this declaration exactly.
enum class Severity : std::uint8_t { Debug, Info, Warning, Error, Critical };

// Human-readable rendering, e.g. "WARNING" for a ConsoleSink line prefix.
// Throws std::invalid_argument for a value outside the enum's declared
// range (e.g. a raw static_cast from an out-of-range integer) rather than
// silently returning a placeholder string - the same "louder failure for a
// caller mistake" reasoning atlas::Context::get<T>() already applies to an
// unregistered PropertyStore.
[[nodiscard]] std::string to_string(Severity severity);

} // namespace atlas::diagnostics
