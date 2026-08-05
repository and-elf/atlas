#include "atlas/render/animation_asset.hpp"

#include <cstring>
#include <limits>

namespace atlas::render {

namespace {

constexpr std::size_t header_size = sizeof(std::uint32_t) * 2;
constexpr std::size_t transform_size = sizeof(float) * 10; // px,py,pz, qx,qy,qz,qw, sx,sy,sz
constexpr std::size_t time_size = sizeof(float);

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

Transform read_transform(std::span<const std::byte> bytes, std::size_t offset) {
    return Transform{
        .position = {read_f32(bytes, offset), read_f32(bytes, offset + 4), read_f32(bytes, offset + 8)},
        .rotation = {read_f32(bytes, offset + 12),
                     read_f32(bytes, offset + 16),
                     read_f32(bytes, offset + 20),
                     read_f32(bytes, offset + 24)},
        .scale = {read_f32(bytes, offset + 28), read_f32(bytes, offset + 32), read_f32(bytes, offset + 36)},
    };
}

} // namespace

std::optional<DecodedAnimation> decode_animation(std::span<const std::byte> bytes) {
    if (bytes.size() < header_size) {
        return std::nullopt;
    }

    const std::uint32_t joint_count = read_u32(bytes, 0);
    const std::uint32_t keyframe_count = read_u32(bytes, sizeof(std::uint32_t));

    // joint_count is u32 multiplying against a fixed per-element size
    // (40 bytes) only, so this product alone can never overflow
    // std::size_t (64-bit on every deployment target this project ships
    // to) - the same "can't overflow" reasoning decode_mesh's own
    // vertex/index counts document.
    const std::size_t joint_transforms_size = static_cast<std::size_t>(joint_count) * transform_size;
    const std::size_t per_keyframe_size = time_size + joint_transforms_size;

    // keyframe_count is a second, independently-declared u32 multiplying
    // against per_keyframe_size (which itself already scales with
    // joint_count) - unlike decode_mesh's counts, which never multiply
    // against each other, two adversarial u32 values here can overflow
    // std::size_t and wrap back around into a small, incorrectly
    // "valid"-looking value, the same failure mode decode_texture's own
    // width * height product guards against. Checked via the identical
    // division-based comparison rather than multiplying directly.
    const std::size_t max_keyframe_count =
        (std::numeric_limits<std::size_t>::max() - header_size) / per_keyframe_size;
    if (static_cast<std::size_t>(keyframe_count) > max_keyframe_count) {
        return std::nullopt;
    }

    const std::size_t needed = header_size + (static_cast<std::size_t>(keyframe_count) * per_keyframe_size);
    if (bytes.size() < needed) {
        return std::nullopt; // truncated/malformed - declared counts run past the end of `bytes`
    }

    DecodedAnimation animation;
    animation.joint_count = joint_count;
    animation.keyframes.reserve(keyframe_count);

    std::size_t offset = header_size;
    for (std::uint32_t keyframe_index = 0; keyframe_index < keyframe_count; ++keyframe_index) {
        AnimationKeyframe keyframe;
        keyframe.time = read_f32(bytes, offset);
        offset += time_size;

        keyframe.joint_transforms.reserve(joint_count);
        for (std::uint32_t joint_index = 0; joint_index < joint_count; ++joint_index) {
            keyframe.joint_transforms.push_back(read_transform(bytes, offset));
            offset += transform_size;
        }

        animation.keyframes.push_back(std::move(keyframe));
    }

    return animation;
}

} // namespace atlas::render
