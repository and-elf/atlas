#include "atlas/depval/validator.hpp"

#include "atlas/cgen/host_composition.hpp"
#include "atlas/cgen/host_manifest.hpp"
#include "atlas/cgen/manifest.hpp"

#include <cstddef>
#include <string>

namespace atlas::depval {

ValidationReport validate_composition(std::string_view host_yaml,
                                      std::span<const std::string_view> capability_yamls) {
    const atlas::cgen::HostManifest host = atlas::cgen::parse_host_manifest(host_yaml);

    std::vector<atlas::cgen::Manifest> capabilities;
    capabilities.reserve(capability_yamls.size());
    for (const auto& capability_yaml : capability_yamls) {
        capabilities.push_back(atlas::cgen::parse_manifest(capability_yaml));
    }

    // resolve_host_composition (tools/atlas-cgen/src/host_composition.cpp) is
    // where the actual dependency-graph merge (explicit depends_on + derived
    // consumes edges), cycle detection, and topological ordering all
    // already live - this call is the entire "validation" this function
    // performs, not a reimplementation of any part of it.
    const atlas::cgen::HostComposition composition =
        atlas::cgen::resolve_host_composition(host, capabilities);

    ValidationReport report{.host_name = composition.host_name, .composition_order = {}};
    report.composition_order.reserve(composition.ordered_capabilities.size());
    for (const auto& manifest : composition.ordered_capabilities) {
        report.composition_order.push_back(manifest.capability_name);
    }
    return report;
}

std::string format_report(const ValidationReport& report) {
    std::string rendered = "host '" + report.host_name + "' composition order (" +
                           std::to_string(report.composition_order.size()) + " capabilities):\n";
    for (std::size_t index = 0; index < report.composition_order.size(); ++index) {
        rendered += "  " + std::to_string(index + 1) + ". " + report.composition_order[index] + "\n";
    }
    return rendered;
}

} // namespace atlas::depval
