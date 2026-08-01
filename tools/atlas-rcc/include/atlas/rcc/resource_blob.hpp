#pragma once

#include "atlas/rcc/resource_table.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace atlas::rcc {

// Packs `entries` into one self-describing binary blob: reads each entry's real
// asset file from `asset_root / entry.path`, then writes
//
//   u64 entry_count
//   entry_count x { u64 id; u64 offset; u64 size }   -- offset/size into the data section below
//   <data section: each entry's file bytes, concatenated in `entries` order>
//
// Integers are written host-native (little-endian on every deployment target
// this project ships to - Debian 13 x86-64, macOS ARM, Windows x86-64 - stated
// explicitly rather than silently assumed, since none of them is big-endian in
// practice). This is a build-time packaging step over trusted local input, so a
// missing/unreadable entry file throws std::invalid_argument - the runtime
// resolution counterpart (atlas::resource::ResourceRegistry) instead reports a
// missing asset as a return value, since that failure is an ordinary condition
// a running host observes, not a build-time authoring error.
//
// atlas-resource's ResourceRegistry independently implements a reader against
// this same documented format rather than depending on this header directly -
// atlas-rcc already depends on atlas::resource for ResourceId, so the reverse
// dependency would be a cycle (spec §5).
//
// Callers decide how to partition entries (typically one blob per asset
// type) - this function is agnostic to that and just packs whatever list it's
// handed.
[[nodiscard]] std::vector<std::byte> pack_resource_blob(const std::vector<CompiledResource>& entries,
                                                        const std::filesystem::path& asset_root);

} // namespace atlas::rcc
