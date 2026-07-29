#pragma once

#include "atlas/contract_gen/manifest.hpp"

#include <string>
#include <string_view>

namespace atlas::contract_gen {

// Emits the generated C++ contract header for a manifest - spec §21's
// Health/ApplyDamage/HealthChanged worked example is this function's ground
// truth. header_name/source_name are cosmetic (the "GENERATED"/"Source:"
// comment lines); callers typically pass the intended output filename and
// the manifest's own source path. Throws std::invalid_argument (see
// manifest.hpp for why this isn't std::expected) if the manifest contains a
// field type outside map_field_type's supported set.
//
// Deliberately does not emit a `static_assert(atlas::DependsOn<...>)` line
// the way §21's illustrative pseudocode shows: that needs a project-wide
// capability dependency graph (§5) this single-manifest generator doesn't
// have. Emitting a hollow, always-true DependsOn concept just to match the
// prose would be worse than omitting it.
[[nodiscard]] std::string
generate_contract(const Manifest& manifest, std::string_view header_name, std::string_view source_name);

} // namespace atlas::contract_gen
