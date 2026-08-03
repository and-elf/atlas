#pragma once

#include "atlas/core/quaternion.hpp"
#include "atlas/core/vec3.hpp"

#include <array>
#include <cmath>

namespace atlas::render {

namespace detail {

// The 3x3 rotation block a unit quaternion produces - shared by
// to_model_matrix (below) and camera.hpp's to_view_matrix, which needs the
// exact same quaternion-to-rotation-matrix math (transposed, for the inverse
// half of a view matrix) rather than a second, independently-derived
// formula that could silently disagree in a rounding corner case. Not part
// of this library's public interface (atlas::render::detail, never
// documented as a stable API) - an implementation-sharing seam only.
struct RotationMatrix3x3 {
    float r00 = 1.0F;
    float r01 = 0.0F;
    float r02 = 0.0F;
    float r10 = 0.0F;
    float r11 = 1.0F;
    float r12 = 0.0F;
    float r20 = 0.0F;
    float r21 = 0.0F;
    float r22 = 1.0F;
};

// Assumes `rotation` is already unit-length, same caveat to_model_matrix's
// own doc comment already documents - a non-unit input produces a non-rigid
// (skewed) rotation block, a caller/precision concern this function does not
// guard against.
[[nodiscard]] constexpr RotationMatrix3x3 to_rotation_matrix(const core::Quaternion& rotation) noexcept {
    const float qx = rotation.x;
    const float qy = rotation.y;
    const float qz = rotation.z;
    const float qw = rotation.w;

    const float xx = qx * qx;
    const float yy = qy * qy;
    const float zz = qz * qz;
    const float xy = qx * qy;
    const float xz = qx * qz;
    const float yz = qy * qz;
    const float wx = qw * qx;
    const float wy = qw * qy;
    const float wz = qw * qz;

    return RotationMatrix3x3{
        .r00 = 1.0F - 2.0F * (yy + zz),
        .r01 = 2.0F * (xy - wz),
        .r02 = 2.0F * (xz + wy),
        .r10 = 2.0F * (xy + wz),
        .r11 = 1.0F - 2.0F * (xx + zz),
        .r12 = 2.0F * (yz - wx),
        .r20 = 2.0F * (xz - wy),
        .r21 = 2.0F * (yz + wx),
        .r22 = 1.0F - 2.0F * (xx + yy),
    };
}

} // namespace detail

// A renderable entity's full spatial state for one resolved tick (spec
// §19: "State -> Renderer -> Output" - Transform is exactly the kind of
// composed property that state consists of, alongside Renderable's
// resource references in renderable.hpp). A basic aggregate (rule of
// zero): no invariant beyond ordinary value semantics.
struct Transform {
    core::Vec3 position;
    core::Quaternion rotation;
    core::Vec3 scale{1.0F, 1.0F, 1.0F};
};

// Linearly interpolates one float component. Computes in double and
// casts back to float explicitly at each step, rather than letting the
// float/double arithmetic mix implicitly - not merely to satisfy this
// project's -Wconversion/-Wdouble-promotion gate, but to make each
// promotion a visible, intentional step rather than an implicit one a
// future edit could silently change.
[[nodiscard]] constexpr float lerp(float start, float end, double alpha) noexcept {
    return static_cast<float>(static_cast<double>(start) +
                              (static_cast<double>(end) - static_cast<double>(start)) * alpha);
}

[[nodiscard]] constexpr core::Vec3
lerp(const core::Vec3& start, const core::Vec3& end, double alpha) noexcept {
    return core::Vec3{
        .x = lerp(start.x, end.x, alpha),
        .y = lerp(start.y, end.y, alpha),
        .z = lerp(start.z, end.z, alpha),
    };
}

// Normalized linear interpolation between two rotations - deliberately
// *not* spherical interpolation (slerp). slerp needs acos/sin, and unlike
// the four arithmetic operations and std::sqrt (which IEEE-754 requires
// to be correctly rounded), the standard does not require transcendental
// functions to be correctly rounded - two conforming platforms' libm are
// free to disagree with each other in the last bit or more, which would
// silently violate spec §4's "FP results must not vary by platform" the
// moment a game asked two different machines to interpolate the same
// rotation. nlerp only needs +, -, *, and std::sqrt, at the cost of
// non-constant angular velocity along the interpolated path - an
// acceptable trade for presentation-only rendering, never something this
// library would accept for simulation state.
[[nodiscard]] inline core::Quaternion
nlerp(const core::Quaternion& start, const core::Quaternion& end, double alpha) noexcept {
    const core::Quaternion raw{
        .x = lerp(start.x, end.x, alpha),
        .y = lerp(start.y, end.y, alpha),
        .z = lerp(start.z, end.z, alpha),
        .w = lerp(start.w, end.w, alpha),
    };

    const double length_squared = (static_cast<double>(raw.x) * static_cast<double>(raw.x)) +
                                  (static_cast<double>(raw.y) * static_cast<double>(raw.y)) +
                                  (static_cast<double>(raw.z) * static_cast<double>(raw.z)) +
                                  (static_cast<double>(raw.w) * static_cast<double>(raw.w));

    // A near-zero sum only arises from interpolating two exactly opposing
    // quaternions (end == -start component-wise) - both represent the
    // same rotation under quaternion double-cover, so falling back to end
    // rather than dividing by ~0 is a safe, deterministic choice, not a
    // meaningfully different rotation being silently substituted.
    constexpr double epsilon = 1e-12;
    if (length_squared < epsilon) {
        return end;
    }

    const double inverse_length = 1.0 / std::sqrt(length_squared);
    return core::Quaternion{
        .x = static_cast<float>(static_cast<double>(raw.x) * inverse_length),
        .y = static_cast<float>(static_cast<double>(raw.y) * inverse_length),
        .z = static_cast<float>(static_cast<double>(raw.z) * inverse_length),
        .w = static_cast<float>(static_cast<double>(raw.w) * inverse_length),
    };
}

// Presentation-only interpolation between two ticks' worth of Transform
// (spec §4, Determinism Constraints: "Wall-clock time may be used only
// for presentation-only concerns (audio/render interpolation)... must
// never feed back into simulation state"). alpha is supplied by the
// caller - this function never reads a clock of any kind, deterministic
// or otherwise; whatever derives alpha (a fixed-timestep accumulator, a
// replay's recorded frame timing) is entirely outside this library's
// concern, per the same spec passage.
[[nodiscard]] inline Transform lerp(const Transform& start, const Transform& end, double alpha) noexcept {
    return Transform{
        .position = lerp(start.position, end.position, alpha),
        .rotation = nlerp(start.rotation, end.rotation, alpha),
        .scale = lerp(start.scale, end.scale, alpha),
    };
}

// Builds a row-major 4x4 model matrix (translation * rotation * scale, for a
// column-vector convention: model * point) from a Transform - issue #154's
// answer to "apply each DrawCommand's Transform as a model matrix". Issue
// #181 adds the missing other half (camera.hpp's to_view_matrix/
// to_projection_matrix) - mesh.vert.hlsl now composes this matrix against
// that real view-projection rather than using it alone.
//
// Row-major (element [row * 4 + col] at index row*4+col, translation in the
// last column of each row) rather than column-major: this project takes no
// third-party math library dependency (matching decode_mesh/decode_texture's
// own hand-rolled-format precedent), so the layout only has to agree with
// itself and with the one HLSL shader that consumes it (mesh.vert.hlsl,
// libraries/atlas-render/shaders/ - declared `row_major`, matching this
// layout exactly, verified against a real SDL_shadercross compile+reflect
// before being committed).
//
// Assumes `transform.rotation` is already unit-length - core::Quaternion
// itself enforces no such invariant (atlas/core/quaternion.hpp's own doc
// comment); a non-unit input produces a non-rigid (skewed) rotation block, a
// caller/precision concern this function does not guard against, the same
// stance nlerp already takes.
[[nodiscard]] constexpr std::array<float, 16> to_model_matrix(const Transform& transform) noexcept {
    const detail::RotationMatrix3x3 r = detail::to_rotation_matrix(transform.rotation);

    const float sx = transform.scale.x;
    const float sy = transform.scale.y;
    const float sz = transform.scale.z;

    // Scale post-multiplies the rotation (scales R's columns, not its rows),
    // then translation occupies the last column - the standard TRS
    // composition for a column-vector transform (model * point).
    return std::array<float, 16>{
        r.r00 * sx,
        r.r01 * sy,
        r.r02 * sz,
        transform.position.x,
        r.r10 * sx,
        r.r11 * sy,
        r.r12 * sz,
        transform.position.y,
        r.r20 * sx,
        r.r21 * sy,
        r.r22 * sz,
        transform.position.z,
        0.0F,
        0.0F,
        0.0F,
        1.0F,
    };
}

} // namespace atlas::render
