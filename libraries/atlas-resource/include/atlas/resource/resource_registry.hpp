#pragma once

#include <atlas/resource/resource_id.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace atlas::resource {

// One compiled resource's resolution data, as atlas-resource itself needs it
// - deliberately independent of atlas::rcc::CompiledResource. atlas-rcc
// already depends on atlas::resource (for ResourceId), so atlas-resource
// depending back on atlas-rcc would be a dependency cycle (spec §5). A
// composing host converts atlas-rcc's compiled ResourceTable into a list of
// these to construct a ResourceRegistry.
struct ResourceEntry {
    ResourceId id;
    std::string type;
    std::string path;
};

enum class ResolutionStatus : std::uint8_t {
    Unresolved,       // id absent from the registry entirely, or a null id
    Resolved,         // present, and the file at its path was read successfully
    ResolutionFailed, // present, but the file could not be read
};

// The outcome of one resolve() call. bytes is only meaningful when status is
// Resolved - matching how ResolvedCue/DrawCommand each report only the
// fields meaningful for their own resolved case, rather than a value/error
// pair.
struct Resolution {
    ResolutionStatus status = ResolutionStatus::Unresolved;
    std::vector<std::byte> bytes;
};

// Resolves ResourceId -> raw asset bytes against a resource root directory
// on disk (never a hard-coded path - spec §3), partitioned by asset type.
//
// Encapsulated (not Rule of Zero) because it protects a real invariant: its
// internal per-type tables must stay consistent with each other and with the
// root they were built against.
//
// Entries are partitioned by `type` once, here at construction - resolve()
// only ever does a partition lookup followed by an id lookup, never a string
// comparison against every entry's type. A capability (atlas-render,
// atlas-audio) resolving its own asset kind therefore never inspects a
// resource's type as a runtime branch; it only ever asks its own partition.
class ResourceRegistry {
public:
    ResourceRegistry(std::vector<ResourceEntry> entries, std::filesystem::path root);

    [[nodiscard]] Resolution resolve(std::string_view type_name, ResourceId id) const;

private:
    std::unordered_map<std::string, std::unordered_map<ResourceId, std::string>> paths_by_type_;
    std::filesystem::path root_;
};

} // namespace atlas::resource
