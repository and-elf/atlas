#pragma once

#include "atlas/rcc/resource_manifest.hpp"
#include <atlas/resource/resource_id.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace atlas::rcc {

// The compiled counterpart of AnimationMetadata: `skeleton` has been
// resolved from a raw resource name to the atlas::ResourceId a running host
// actually looks resources up by - the same resolution `entry.name` itself
// gets in compile_resource_table, since a skeleton reference needs to
// resolve against another compiled resource, not just carry a name through
// blind (issue #45). Whether that name actually corresponds to some other
// entry in the same manifest is deliberately not checked here (or anywhere
// in this tool) - see resource_table.cpp's compile_resource_table comment
// and README's "no path existence checking" precedent; that is a
// resolution-time (running host) concern, not a build-time one.
struct CompiledAnimationMetadata {
    atlas::ResourceId skeleton;
    bool loop = false;
    double playback_rate = 1.0;
};

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
    std::optional<CompiledAnimationMetadata> animation;
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
//
// An entry carrying AnimationMetadata has its `skeleton` name resolved
// through the same atlas::ResourceId::from_name as entry.name itself,
// producing a CompiledAnimationMetadata - never checked here against
// whether that name actually names another entry in this same manifest
// (a resolution-time host concern, not this build-time step's job).
[[nodiscard]] ResourceTable compile_resource_table(const std::vector<ResourceEntry>& entries);

} // namespace atlas::rcc
