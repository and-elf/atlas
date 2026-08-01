#pragma once

#include "atlas/refl/manifest.hpp"

#include <string>
#include <string_view>

namespace atlas::refl {

// Renders a Manifest into a generated reflection-metadata header: one
// `constexpr std::array<atlas::refl::FieldMetadata, N>` per property/
// request/event struct the manifest declares, in declaration order
// (properties, then requests, then events - matching
// tools/atlas-cgen/src/contract_writer.cpp's generate_contract emission
// order exactly, so the two generators' output orders stay predictable
// relative to each other), plus a `constexpr std::string_view` composition
// constant for a property that declares a `composition:` key (spec §20).
//
// header_name/source_name are used only for the file's own "// GENERATED"
// banner comment (see tools/atlas-cgen's contract file template for the
// precedent this mirrors) - they do not affect the emitted namespace or
// struct names, which come entirely from manifest.capability_name and each
// StructDecl::name.
//
// Throws std::invalid_argument if a field's type or a property's
// composition strategy isn't in this generator's closed, mapped set (see
// map_field_type/map_composition_strategy in manifest.hpp) - a defensive
// check against a hand-built Manifest bypassing parse_manifest's own
// validation, not just trusting an already-validated caller (mirroring
// generate_contract's own equivalent defensive check).
[[nodiscard]] std::string generate_reflection_metadata(const Manifest& manifest,
                                                       std::string_view header_name,
                                                       std::string_view source_name);

} // namespace atlas::refl
