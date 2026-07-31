#pragma once

#include "atlas/rcc/resource_manifest.hpp"
#include <atlas/resource/resource_id.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace atlas::rcc {

// The resolution data a downstream host needs for one compiled resource:
// its authored name (kept alongside the id for diagnostics - the id itself
// doesn't carry it back out), its asset type tag, and the relative path a
// future resource-loading mechanism resolves against. Never a hard-coded
// absolute path (spec §3, Resource) - `path` is exactly the string an
// artist/author wrote in the manifest, carried through unchanged.
//
// Rule of Zero: a plain aggregate, no invariant of its own to protect - the
// invariants (non-empty fields, name uniqueness) are enforced once, up
// front, by parse_resource_manifest/compile_resource_table, not by this
// value type re-checking itself on every use.
struct CompiledResource {
    atlas::ResourceId id;
    std::string name;
    std::string type;
    std::string path;
};

// The compiled output of this tool (spec §12: "resource compilation... a
// compiled table of ResourceId plus whatever resolution data downstream
// needs"): every authored resource, keyed by the same atlas::ResourceId a
// host looks it up with at runtime. unordered_map is the correct shape here,
// not a design compromise - lookup is by id, never by declaration order,
// and atlas::ResourceId already has the std::hash specialization
// (resource_id.hpp) this needs.
using ResourceTable = std::unordered_map<atlas::ResourceId, CompiledResource>;

// Compiles parsed resource entries (already validated for duplicate names by
// parse_resource_manifest) into a ResourceTable keyed by
// atlas::ResourceId::from_name(entry.name). Does not re-validate name
// uniqueness itself - that is parse_resource_manifest's job, and duplicating
// the check here would just be re-detecting the same invariant twice; a
// caller building a ResourceTable from entries it produced some other way is
// responsible for the same uniqueness guarantee (see README's open
// questions for the one edge case this deliberately leaves unhandled: a
// genuine ResourceId hash collision between two distinct, non-duplicate
// names).
[[nodiscard]] ResourceTable compile_resource_table(const std::vector<ResourceEntry>& entries);

} // namespace atlas::rcc
