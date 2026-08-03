#pragma once

#include <string_view>

namespace atlas::demo {

// issue #200: the player entity's real (placeholder) mesh/texture resource
// names - the stable identity atlas::ResourceId::from_name hashes, matching
// exactly what demo/resources/manifest.yaml declares and what
// demo/resources/Mesh.blob/Texture.blob (packed via atlas-rcc from
// demo/resources/raw/player.mesh/player.tex) were built from. A shared
// header rather than inlining the literal in each call site, since both
// PresentationApp (seeding the player's Renderable) and main.cpp
// (constructing the real ResourceRegistry) - plus tests proving the whole
// pipeline resolves - need the exact same names.
inline constexpr std::string_view kPlayerMeshResourceName = "characters/player/mesh";
inline constexpr std::string_view kPlayerTextureResourceName = "characters/player/texture";

} // namespace atlas::demo
