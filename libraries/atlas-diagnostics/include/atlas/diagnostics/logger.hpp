#pragma once

#include "atlas/diagnostics/record.hpp"
#include "atlas/diagnostics/severity.hpp"
#include "atlas/diagnostics/sink.hpp"

#include <string>
#include <utility>
#include <vector>

namespace atlas::diagnostics {

// Fans a Record out to every registered Sink, in registration order (spec
// §4, Deterministic Execution: never unordered iteration over registered
// work) - the "one mechanism, reused everywhere" spec §6 asks the uniform
// failure channel to be. Logger itself has no opinion on what counts as a
// failure or which systems report through it (see this library's README,
// "Kept decoupled from any specific runtime failure type") - it only
// distributes whatever Record it is given.
//
// IMPORTANT - presentation/diagnostic-only (spec §4; see this library's
// README, "Deterministic-safe"): logging a Record, and which sinks are
// registered, must never be read back by simulation logic to decide
// anything. Logger deliberately exposes no way to query what has already
// been logged - only log() (write-only) and add_sink() (registration) -
// so there is no read path simulation code could accidentally come to
// depend on.
//
// An encapsulated class rather than a Rule-of-Zero aggregate: it protects
// the invariant that sinks_ is only ever appended to through add_sink(),
// never reordered - matching atlas::scheduler::Scheduler's own reasoning
// for why deterministic dispatch order needs an owned, private container
// rather than a public field a caller could rearrange.
class Logger {
public:
    // Registers sink to receive every subsequent log() call, for as long
    // as sink itself stays alive. A non-owning pointer, mirroring
    // atlas::Context::register_property_store<T>() - whoever composes a
    // host owns the actual Sink instances (a ConsoleSink, a future file
    // sink) and decides their lifetime; Logger only coordinates dispatch
    // to them (CLAUDE.md, Architecture Principles: dependency injection).
    void add_sink(Sink& sink) { sinks_.push_back(&sink); }

    // Dispatches record to every registered sink, in registration order.
    // Harmless (not an error) when no sink is registered yet, the same
    // way atlas::Context::publish<T>() treats "nobody is listening" as an
    // ordinary case rather than a setup mistake.
    void log(const Record& record) const {
        for (auto* sink : sinks_) {
            sink->write(record);
        }
    }

    // Convenience overload building a Record inline - the common call
    // shape a reporting system uses (spec §6's "what failed, which system
    // reported it, system-specific detail", in that order).
    void log(Severity severity,
             std::string system,
             std::string message,
             std::vector<DetailField> details = {}) const {
        log(Record{.severity = severity,
                   .system = std::move(system),
                   .message = std::move(message),
                   .details = std::move(details)});
    }

private:
    std::vector<Sink*> sinks_;
};

} // namespace atlas::diagnostics
