#pragma once

#include "atlas/cgen/manifest.hpp"

#include <string>
#include <string_view>

namespace atlas::docgen {

// Emits a human-readable Markdown reference page for a single capability
// manifest (spec §12's "documentation generation" Atlas Tooling
// responsibility) - describing the capability's name, its
// depends_on/consumes dependencies, and its properties/requests/events
// (each with composition/trigger annotations where declared, spec §20).
//
// Reuses atlas::cgen::Manifest (atlas-cgen's own parsed structure) rather
// than parsing manifests a second time - see tools/atlas-docgen/README.md's
// "Dependency position" for why this links atlas-cgen-lib instead of
// reimplementing YAML parsing.
//
// header_name/source_name are cosmetic (the "GENERATED"/"Source:" comment
// line), mirroring atlas::cgen::generate_contract's own two-name signature:
// callers typically pass the intended output filename and the manifest's
// own source path.
//
// Field types are rendered exactly as declared in the manifest (e.g.
// "int32", "EntityRef") rather than their mapped C++ spelling
// (std::int32_t, atlas::EntityRef) - this is a reference document about the
// manifest's own declared structure, not the generated C++ contract, so the
// manifest's own vocabulary is what a capability author should see. Every
// field type is already guaranteed to be one atlas::cgen::map_field_type
// recognizes, since parse_manifest validates that up front - this function
// does not re-validate it.
[[nodiscard]] std::string generate_markdown_doc(const atlas::cgen::Manifest& manifest,
                                                std::string_view header_name,
                                                std::string_view source_name);

} // namespace atlas::docgen
