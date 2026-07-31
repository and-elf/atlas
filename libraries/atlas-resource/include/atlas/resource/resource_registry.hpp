#pragma once

#include <atlas/resource/resource_id.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace atlas::resource {

enum class ResolutionStatus : std::uint8_t {
    Unresolved,       // type never registered, or the type's blob doesn't contain this id, or a null id
    Resolved,         // the type's blob loaded successfully and contains this id
    ResolutionFailed, // the type was registered, but its blob failed to load or parse
};

// The outcome of one resolve() call. bytes is only meaningful when status is
// Resolved - matching how ResolvedCue/DrawCommand each report only the
// fields meaningful for their own resolved case, rather than a value/error
// pair.
struct Resolution {
    ResolutionStatus status = ResolutionStatus::Unresolved;
    std::vector<std::byte> bytes;
};

// Resolves ResourceId -> raw asset bytes against packed, in-memory blobs - one
// per asset type - rather than loose files on disk. Every blob named in
// `blob_paths_by_type` is read fully into memory exactly once, here at
// construction: resolve() afterward never touches the filesystem at all, only
// an in-memory index lookup plus a byte-range copy. This is what actually
// avoids "hitting the filesystem every time a texture loads" - not literal
// binary embedding (see the library README's Scoping decisions for why that
// was rejected in favor of this).
//
// Each blob is the self-describing binary format packed by
// atlas::rcc::pack_resource_blob (tools/atlas-rcc/include/atlas/rcc/resource_blob.hpp)
// - see that function's documentation for the exact layout. Deliberately
// re-implemented here rather than shared via a common header: atlas-rcc
// already depends on atlas::resource (for ResourceId), so atlas-resource
// depending back on atlas-rcc would be a dependency cycle (spec §5).
//
// Encapsulated (not Rule of Zero) because it protects a real invariant: each
// loaded blob's byte buffer and its index must stay consistent with each
// other for the lifetime of the registry.
class ResourceRegistry {
public:
    explicit ResourceRegistry(
        const std::unordered_map<std::string, std::filesystem::path>& blob_paths_by_type);

    [[nodiscard]] Resolution resolve(std::string_view type_name, ResourceId id) const;

private:
    struct BlobEntry {
        std::uint64_t offset = 0; // absolute into LoadedBlob::bytes
        std::uint64_t size = 0;
    };

    struct LoadedBlob {
        std::vector<std::byte> bytes;
        std::unordered_map<ResourceId, BlobEntry> index;
    };

    // std::nullopt: the blob at this path failed to load or was malformed.
    [[nodiscard]] static std::optional<LoadedBlob> load_blob(const std::filesystem::path& path);

    // Absent key: type never registered (Unresolved for any id under it).
    // Present, std::nullopt: a blob path was given for this type but failed
    // to load/parse (ResolutionFailed for any id under it - the registry
    // knows resources of this type were compiled, it just can't get at the
    // data). Present with a value: successfully loaded.
    std::unordered_map<std::string, std::optional<LoadedBlob>> blobs_by_type_;
};

} // namespace atlas::resource
