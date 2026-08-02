#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace atlas::depval {

// A validated host composition, reduced to exactly what a standalone
// validator needs to report (spec §5, Ordering Without Stages): the host's
// own name and its full topological composition order, by capability name.
// Deliberately smaller than atlas::cgen::HostComposition (which carries a
// complete atlas::cgen::Manifest - properties/requests/events - per
// composed capability, since it feeds contract-generation codegen this tool
// never performs) - this type is this tool's own reporting shape, not a
// borrowed generation-oriented one.
struct ValidationReport {
    std::string host_name;
    std::vector<std::string> composition_order;
};

// Parses `host_yaml` (atlas::cgen::parse_host_manifest) and every entry of
// `capability_yamls` (atlas::cgen::parse_manifest), then resolves the
// composed dependency graph via atlas::cgen::resolve_host_composition - the
// full merged `depends_on` + `consumes:`-derived edge set (spec §5), never
// `depends_on` alone, exactly as atlas-cgen's own host composition mode
// already validates it. This function does not reimplement any of that
// resolution or cycle-detection logic; it is the standalone-validator-
// shaped entry point around it, returning only the ordering a caller who
// wants *validation*, not code generation, actually needs.
//
// Every error atlas::cgen::parse_host_manifest / atlas::cgen::parse_manifest
// / atlas::cgen::resolve_host_composition can throw propagates unchanged -
// including atlas::cgen::DependencyCycleError, whose message names the full
// chain of edges forming a cycle, in order, not just the first offending
// dependency (spec §5's explicit requirement).
[[nodiscard]] ValidationReport validate_composition(std::string_view host_yaml,
                                                    std::span<const std::string_view> capability_yamls);

// Renders a ValidationReport as the human-readable text atlas-depval prints
// to stdout on success: the host name, the composed capability count, and
// one ordered line per composed capability. A host composing nothing (an
// empty composition_order) still names the host, with zero ordered lines,
// rather than being treated as an error - resolve_host_composition itself
// already accepts an empty `composes:` list as a legitimate, if degenerate,
// case.
[[nodiscard]] std::string format_report(const ValidationReport& report);

} // namespace atlas::depval
