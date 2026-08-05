#include "atlas/render/skeleton_asset.hpp"

#include <cstring>

namespace atlas::render {

namespace {

constexpr std::size_t header_size = sizeof(std::uint32_t);
constexpr std::size_t joint_size = sizeof(std::uint32_t) + (sizeof(float) * 10); // parent, pos, rot, scale

std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

float read_f32(std::span<const std::byte> bytes, std::size_t offset) {
    float value = 0.0F;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

} // namespace

std::optional<SkeletonAsset> decode_skeleton(std::span<const std::byte> bytes) {
    if (bytes.size() < header_size) {
        return std::nullopt;
    }

    const std::uint32_t joint_count = read_u32(bytes, 0);

    // joint_count is u32 and only ever multiplies against a small fixed
    // per-element size (44 bytes), never against another declared count -
    // the same "can't overflow std::size_t" reasoning decode_mesh's own
    // vertex/index counts already document (mesh_asset.cpp).
    const std::size_t joint_bytes = static_cast<std::size_t>(joint_count) * joint_size;
    const std::size_t needed = header_size + joint_bytes;
    if (bytes.size() < needed) {
        return std::nullopt; // truncated/malformed - declared joint count runs past the end of `bytes`
    }

    SkeletonAsset skeleton;
    skeleton.joints.reserve(joint_count);
    std::size_t offset = header_size;
    for (std::uint32_t joint_index = 0; joint_index < joint_count; ++joint_index) {
        const std::uint32_t parent_index = read_u32(bytes, offset);

        // Required hierarchy invariant (skeleton_asset.hpp's own doc
        // comment): a parent must either be absent or precede this joint in
        // authoring order. Rejecting here, before any joint is committed to
        // the output, is what makes the hierarchy acyclic and in-range by
        // construction rather than needing a separate graph traversal once
        // decoding has already finished.
        if (parent_index != kNoParentJoint && parent_index >= joint_index) {
            return std::nullopt;
        }

        skeleton.joints.push_back(Joint{
            .parent_index = parent_index,
            .bind_pose =
                Transform{
                    .position = {read_f32(bytes, offset + 4),
                                 read_f32(bytes, offset + 8),
                                 read_f32(bytes, offset + 12)},
                    .rotation = {read_f32(bytes, offset + 16),
                                 read_f32(bytes, offset + 20),
                                 read_f32(bytes, offset + 24),
                                 read_f32(bytes, offset + 28)},
                    .scale = {read_f32(bytes, offset + 32),
                              read_f32(bytes, offset + 36),
                              read_f32(bytes, offset + 40)},
                },
        });
        offset += joint_size;
    }

    return skeleton;
}

} // namespace atlas::render
