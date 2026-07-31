#pragma once

#include "atlas/diagnostics/severity.hpp"

#include <string>
#include <vector>

namespace atlas::diagnostics {

// One key-value pair of system-specific detail (spec §6: "system-specific
// detail relevant to diagnosing it - e.g. the resource identifier that
// failed to resolve, or the two contract versions that mismatched"). A
// plain aggregate (Rule of Zero): no invariant to protect, so comparison
// defaults rather than being hand-rolled.
//
// value is always a string, deliberately - this library does not attempt
// a typed variant (int/bool/EntityRef/...) payload. A reporting system
// that has, say, a numeric resource identifier is expected to render it
// (std::to_string, or whatever formatting it already uses) before handing
// it here; keeping the value type fixed keeps Record itself simple, and a
// diagnostic payload's whole purpose is to end up as human/tool-readable
// text (console, log file, future replay-tool query) rather than to
// round-trip as a typed value the way a replicated property does.
struct DetailField {
    std::string key{};   // NOLINT(readability-redundant-member-init)
    std::string value{}; // NOLINT(readability-redundant-member-init)

    friend bool operator==(const DetailField&, const DetailField&) = default;
};

// One structured diagnostic occurrence - spec §6's "common failure
// structure: what failed, which system reported it, and system-specific
// detail," generalized slightly so a system can also emit a non-failure
// diagnostic (Severity::Debug/Info) through the same mechanism, not only
// failures. A plain aggregate (Rule of Zero): nothing here protects an
// invariant across its own operations.
//
// details is an ordered std::vector<DetailField>, not a
// std::unordered_map<std::string, std::string>: iteration order over an
// unordered_map is an implementation detail that can differ by standard
// library/hash seed. A diagnostic Record is deliberately kept out of
// simulation state (see this library's README, "Deterministic-safe"), so
// that difference could never affect a simulation result - but
// reproducible field order still matters for this library's own stated
// purpose (structured, queryable output a future tool greps/diffs) and
// for tests asserting exact rendered output. A vector preserves whatever
// order the reporting system inserted its details in, at the cost of
// O(n) lookup by key - a cost nothing here pays, since every consumer
// (ConsoleSink, a future file sink) iterates the full set rather than
// looking up one detail by key among many.
struct Record {
    Severity severity = Severity::Info;

    // Which runtime system reported this (spec §6: "which system reported
    // it") - e.g. "resource", "replication", "request". A plain string
    // rather than an enum: this library deliberately does not enumerate
    // the set of systems that can report (see README, "Kept decoupled
    // from any specific runtime failure type") - each reporting system
    // names itself.
    std::string system{}; // NOLINT(readability-redundant-member-init)

    // What failed (or, for a non-failure diagnostic, what happened) -
    // spec §6: "what failed."
    std::string message{}; // NOLINT(readability-redundant-member-init)

    // The `{}`s above and below are redundant from std::string/
    // std::vector's own default constructors' point of view
    // (readability-redundant-member-init), but removing them reopens
    // GCC's -Wmissing-field-initializers at every call site that
    // designated-initializes only a subset of Record's fields (e.g.
    // `Record{.severity = Severity::Error}`) - see commit "Silence
    // readability-redundant-member-init without losing
    // -Wmissing-field-initializers" (atlas-ui) for the same fix applied
    // here first.
    std::vector<DetailField> details{}; // NOLINT(readability-redundant-member-init)

    friend bool operator==(const Record&, const Record&) = default;
};

} // namespace atlas::diagnostics
