#pragma once

#include <atlas/resource/resource_id.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
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

enum class ResourceChangeKind : std::uint8_t {
    Reloaded,     // the type's blob file changed on disk and reloaded successfully
    ReloadFailed, // the type's blob file changed on disk but failed to reload/parse
};

// One type's blob changed since the previous poll_for_changes() call -
// consumers (e.g. atlas-render) that hold onto a previously-returned
// Resolution for a ResourceId under this type should treat it as stale.
// Named by type_name only, not by individual ResourceId: this library
// resolves and reloads against a whole type's packed blob at once, never a
// single entry within it, so whole-blob granularity is the finest this
// event can honestly report. Rule of Zero: plain data, no invariant.
struct ResourceChangeEvent {
    std::string type_name;
    ResourceChangeKind kind = ResourceChangeKind::Reloaded;
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

    // Development/editor-only hot-reload (issue #67), opt-in purely by
    // whether a caller ever invokes this - a headless production host that
    // never calls poll_for_changes() pays nothing beyond the one
    // std::filesystem::last_write_time query per registered type already
    // done once at construction. Real filesystem I/O here is not a §4
    // determinism violation (see the library README's Scoping decisions).
    //
    // Checks every registered type's blob file's last-write-time via
    // std::filesystem::last_write_time - no background thread, no signal
    // handler, nothing happens except when the caller explicitly calls this
    // (mirroring atlas::replication::Transport::poll_received()'s own
    // drain-the-batch-per-call precedent, not a callback/observer). A type
    // whose write time differs from what was last observed (at construction,
    // or the previous poll_for_changes() call) is reloaded immediately via
    // load_blob(), replacing its stale LoadedBlob in place so a subsequent
    // resolve() call returns fresh bytes - resolve() itself never reloads
    // anything, staying a pure in-memory lookup exactly as before. Reload
    // failure (blob now missing, or malformed the same way the constructor
    // already handles) is reported as ResourceChangeKind::ReloadFailed, not
    // an exception, matching load_blob()'s own return-value convention; a
    // subsequent resolve() call for that type then reports
    // ResolutionStatus::ResolutionFailed exactly as it would for a blob that
    // failed to load at construction.
    //
    // The returned span aliases this instance's own internal buffer, valid
    // until the next poll_for_changes() call - overwritten, not appended to,
    // each call (matching UnixSocketTransport::poll_received()'s identical
    // convention). Events are sorted by type_name so the return order is
    // deterministic regardless of the unordered_map's own iteration order.
    [[nodiscard]] std::span<const ResourceChangeEvent> poll_for_changes();

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

    // Retained (beyond construction) solely so poll_for_changes() knows
    // which path to re-check/reload per type.
    std::unordered_map<std::string, std::filesystem::path> blob_paths_by_type_;

    // The last last-write-time poll_for_changes() (or the constructor, for
    // the first poll) observed for each type's blob path. std::nullopt
    // means the path didn't exist (or wasn't queryable) at that observation
    // - itself a valid, trackable state so a file's later appearance is
    // also detected as a change.
    std::unordered_map<std::string, std::optional<std::filesystem::file_time_type>> last_write_time_by_type_;

    std::vector<ResourceChangeEvent> pending_changes_;
};

} // namespace atlas::resource
