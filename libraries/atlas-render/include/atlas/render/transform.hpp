#pragma once

#include <array>
#include <cmath>

namespace atlas::render {

// A 3D position, direction, or scale, in whatever units a composing
// game's own convention uses - atlas-render has no opinion on that (spec
// §2, Mechanism Over Meaning). float, not double: this is presentation-
// facing render data, the conventional precision a GPU vertex pipeline
// expects, not simulation state itself. A basic aggregate (rule of zero):
// no invariant beyond ordinary value semantics.
struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

// A rotation, represented as a quaternion rather than Euler angles -
// avoids gimbal lock and composes predictably under interpolation, the
// conventional choice for a 3D renderer's transform. The default value
// (0, 0, 0, 1) is the identity rotation. A basic aggregate (rule of
// zero): no invariant enforced by this type itself - a Quaternion built
// or interpolated into a non-unit length is a caller/precision concern,
// not something the type constructs against.
struct Quaternion {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 1.0F;
};

// A renderable entity's full spatial state for one resolved tick (spec
// §19: "State -> Renderer -> Output" - Transform is exactly the kind of
// composed property that state consists of, alongside Renderable's
// resource references in renderable.hpp). A basic aggregate (rule of
// zero): no invariant beyond ordinary value semantics.
struct Transform {
    Vec3 position;
    Quaternion rotation;
    Vec3 scale{1.0F, 1.0F, 1.0F};
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

[[nodiscard]] constexpr Vec3 lerp(const Vec3& start, const Vec3& end, double alpha) noexcept {
    return Vec3{
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
[[nodiscard]] inline Quaternion nlerp(const Quaternion& start, const Quaternion& end, double alpha) noexcept {
    const Quaternion raw{
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
    return Quaternion{
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
// answer to "apply each DrawCommand's Transform as a model matrix": no
// Camera/view-projection concept exists anywhere in Atlas yet (see this
// library's README, "Open questions" - a real Camera is an explicit
// follow-up, not this round's job), so this matrix is pushed to the vertex
// shader and used alone, with nothing composed against it.
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
// Assumes `transform.rotation` is already unit-length - Quaternion itself
// enforces no such invariant (transform.hpp's own doc comment on Quaternion);
// a non-unit input produces a non-rigid (skewed) rotation block, a caller/
// precision concern this function does not guard against, the same stance
// nlerp already takes.
[[nodiscard]] constexpr std::array<float, 16> to_model_matrix(const Transform& transform) noexcept {
    const float qx = transform.rotation.x;
    const float qy = transform.rotation.y;
    const float qz = transform.rotation.z;
    const float qw = transform.rotation.w;

    const float xx = qx * qx;
    const float yy = qy * qy;
    const float zz = qz * qz;
    const float xy = qx * qy;
    const float xz = qx * qz;
    const float yz = qy * qz;
    const float wx = qw * qx;
    const float wy = qw * qy;
    const float wz = qw * qz;

    const float r00 = 1.0F - 2.0F * (yy + zz);
    const float r01 = 2.0F * (xy - wz);
    const float r02 = 2.0F * (xz + wy);
    const float r10 = 2.0F * (xy + wz);
    const float r11 = 1.0F - 2.0F * (xx + zz);
    const float r12 = 2.0F * (yz - wx);
    const float r20 = 2.0F * (xz - wy);
    const float r21 = 2.0F * (yz + wx);
    const float r22 = 1.0F - 2.0F * (xx + yy);

    const float sx = transform.scale.x;
    const float sy = transform.scale.y;
    const float sz = transform.scale.z;

    // Scale post-multiplies the rotation (scales R's columns, not its rows),
    // then translation occupies the last column - the standard TRS
    // composition for a column-vector transform (model * point).
    return std::array<float, 16>{
        r00 * sx,
        r01 * sy,
        r02 * sz,
        transform.position.x,
        r10 * sx,
        r11 * sy,
        r12 * sz,
        transform.position.y,
        r20 * sx,
        r21 * sy,
        r22 * sz,
        transform.position.z,
        0.0F,
        0.0F,
        0.0F,
        1.0F,
    };
}

} // namespace atlas::render
