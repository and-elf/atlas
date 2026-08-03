#pragma once

#include "atlas/core/quaternion.hpp"
#include "atlas/core/vec3.hpp"
#include "atlas/render/transform.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace atlas::render {

// Issue #181: the missing other half of transform.hpp's to_model_matrix -
// "no Camera/view-projection concept exists anywhere in Atlas yet" was
// flagged and deliberately deferred twice (issues #154, #156 - see both
// README "Open questions" sections). Presentation-only per §4/§20 ("view/
// projection... presentation concern"): a Camera never feeds back into
// simulation state, the same carve-out transform.hpp's own lerp/nlerp
// already document for interpolation. A basic aggregate (rule of zero): no
// invariant either field group enforces itself, mirroring Transform's own
// stance - `orientation` a non-unit quaternion, or `near_clip >= far_clip`,
// are both caller/precision concerns this type does not guard against.
//
// `position`/`orientation` describe the camera's own placement in world
// space, the exact same representation (core::Vec3/core::Quaternion) a
// Transform uses for a renderable entity - to_view_matrix (below) treats
// them as a Transform lacking only `scale` (a camera is never scaled).
// `vertical_fov_radians`/`aspect_ratio`/`near_clip`/`far_clip` describe the
// projection: vertical field of view in radians (not degrees - this
// project's own quaternion/rotation math throughout, e.g.
// sdl3_spinning_box_test.cpp's rotation_for_tick, already works in radians
// directly; storing degrees here would be the one place in this library
// that didn't, needing a conversion at every call site for no benefit),
// `aspect_ratio` as width/height, `near_clip`/`far_clip` as positive
// distances along the camera's own forward axis (near_clip < far_clip).
//
// Defaults describe a generic, immediately-usable camera - not tuned for
// any specific scene: positioned at the world origin, identity orientation
// (looking down its own local +Z axis - see to_view_matrix's own doc
// comment for why +Z, not the OpenGL-style -Z convention), a 60-degree
// vertical FOV (a common, unsurprising default for a first-person/third-
// person camera), a 16:9 aspect ratio (an ordinary default a caller
// composing a differently-shaped window should override to match its own
// window's actual width/height - this type has no way to discover that
// itself), and a 0.1-to-1000-unit near/far range (generous enough that
// content authored near the origin, the same assumption
// sdl3_distance_cull_pipeline.hpp's own DistanceCullConfig default already
// makes for its own 1000-unit max_distance, stays within view).
struct Camera {
    core::Vec3 position;
    core::Quaternion orientation;
    float vertical_fov_radians = std::numbers::pi_v<float> / 3.0F; // 60 degrees.
    float aspect_ratio = 16.0F / 9.0F;
    float near_clip = 0.1F;
    float far_clip = 1000.0F;
};

// Builds a row-major 4x4 view matrix - the inverse of the camera's own world
// transform (position + orientation, no scale) - from a Camera. Same
// column-vector convention and row-major flat-array layout as
// transform.hpp's to_model_matrix (element [row * 4 + col] at index
// row*4+col; must agree with mesh.vert.hlsl's `row_major` declaration for
// the same reason to_model_matrix's own doc comment gives).
//
// For a rotation+translation-only transform world = Translate(position) *
// Rotate(orientation) (i.e. world_point = R * local_point + position), the
// inverse mapping a world-space point into the camera's own local/view
// space is view_point = R^-1 * (world_point - position). R is orthonormal
// for a unit quaternion (to_rotation_matrix's own documented assumption,
// inherited here unchanged), so R^-1 == R^T (transpose) - never simply
// negating position and orientation independently, a common and subtly
// wrong shortcut this doc comment calls out explicitly per issue #181's own
// review guidance. Expanded: view_point = R^T * world_point - R^T *
// position, which is exactly Transform-shaped (a rotation block plus a
// translation column) with the rotation block transposed and the
// translation column computed from -R^T * position rather than taken
// directly from `position` the way to_model_matrix's own translation column
// is.
//
// Reuses transform.hpp's detail::to_rotation_matrix - the exact same
// quaternion-to-rotation-matrix formula to_model_matrix itself uses, not a
// second, independently re-derived one that could silently disagree with it
// in a rounding corner case.
[[nodiscard]] constexpr std::array<float, 16> to_view_matrix(const Camera& camera) noexcept {
    const detail::RotationMatrix3x3 r = detail::to_rotation_matrix(camera.orientation);

    const float px = camera.position.x;
    const float py = camera.position.y;
    const float pz = camera.position.z;

    // R^T's row i is R's column i - transposing by construction rather than
    // computing R then transposing it as a separate step.
    const float t0 = -((r.r00 * px) + (r.r10 * py) + (r.r20 * pz));
    const float t1 = -((r.r01 * px) + (r.r11 * py) + (r.r21 * pz));
    const float t2 = -((r.r02 * px) + (r.r12 * py) + (r.r22 * pz));

    return std::array<float, 16>{
        r.r00,
        r.r10,
        r.r20,
        t0,
        r.r01,
        r.r11,
        r.r21,
        t1,
        r.r02,
        r.r12,
        r.r22,
        t2,
        0.0F,
        0.0F,
        0.0F,
        1.0F,
    };
}

// Builds a row-major 4x4 perspective projection matrix from a Camera's
// projection parameters, targeting SDL_GPU's own documented clip-space
// convention exactly (SDL_gpu.h, "Coordinate System", verified against the
// real fetched header before writing this function, not assumed): "a
// left-handed coordinate system, following the convention of D3D12 and
// Metal... Z values range from [0.0, 1.0] where 0 is the near plane."
// Reusing D3D12's own convention end to end (left-handed view space, camera
// forward = its own local +Z axis, matching to_view_matrix's identity-
// orientation basis above) avoids introducing a handedness flip anywhere
// between view and clip space - the simplest choice that is also directly
// consistent with what SDL_GPU itself already documents needing.
//
// Column-vector convention (clip = P * view_point), matching
// mesh.vert.hlsl's `mul(matrix, vector)` call and to_model_matrix's own
// row-major layout - NOT the row-vector convention (view_point * M) D3D's
// own XMMatrixPerspectiveFovLH documentation traditionally presents its
// formula in. This function's matrix is that traditional matrix's
// transpose, independently re-derived and verified here (not copied) by
// checking the resulting NDC depth at z_view == near_clip and z_view ==
// far_clip lands at exactly 0 and 1 respectively (see camera_test.cpp) -
// the same double-check performed before committing this convention choice:
//
//   xScale = 1 / (aspect_ratio * tan(vertical_fov_radians / 2))
//   yScale = 1 / tan(vertical_fov_radians / 2)
//   clip.x = xScale * view.x
//   clip.y = yScale * view.y
//   clip.z = (far_clip / (far_clip - near_clip)) * view.z
//            - (near_clip * far_clip / (far_clip - near_clip))
//   clip.w = view.z
//
// clip.w == view.z (not -view.z, the RH/OpenGL convention's usual
// construction) is the direct consequence of choosing forward == +Z: a
// visible point already has a positive view-space Z, so it already has a
// positive clip.w without needing a sign flip.
//
// Not constexpr (unlike to_view_matrix/to_model_matrix): std::tan is not a
// core constant expression in C++23, the same "correctly-rounded but not a
// constant expression" situation transform.hpp's own nlerp already
// documents for std::sqrt - inline, not constexpr, is this project's
// established answer to that (see nlerp's own doc comment).
[[nodiscard]] inline std::array<float, 16> to_projection_matrix(const Camera& camera) noexcept {
    const float y_scale = 1.0F / std::tan(camera.vertical_fov_radians * 0.5F);
    const float x_scale = y_scale / camera.aspect_ratio;
    const float near_clip = camera.near_clip;
    const float far_clip = camera.far_clip;
    const float depth_range = far_clip - near_clip;
    const float z_scale = far_clip / depth_range;
    const float z_translate = -(near_clip * far_clip) / depth_range;

    return std::array<float, 16>{
        x_scale,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        y_scale,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        z_scale,
        z_translate,
        0.0F,
        0.0F,
        1.0F,
        0.0F,
    };
}

namespace detail {

// Multiplies two row-major 4x4 matrices (lhs * rhs, column-vector
// convention) - the one piece of generic matrix arithmetic
// to_view_projection_matrix (below) needs that neither to_model_matrix nor
// to_view_matrix required on their own (both build a matrix directly from a
// rotation/translation, never combine two already-built ones). A plain,
// unrolled triple loop - no third-party math library dependency, matching
// this project's own established stance (transform.hpp's own doc comment,
// decode_mesh/decode_texture's hand-rolled formats). Not part of this
// library's public interface (atlas::render::detail, same seam
// to_rotation_matrix above already establishes) - a caller wanting a
// composed view-projection matrix should use to_view_projection_matrix
// itself, not this generic helper directly.
[[nodiscard]] constexpr std::array<float, 16> multiply_matrices(const std::array<float, 16>& lhs,
                                                                const std::array<float, 16>& rhs) noexcept {
    std::array<float, 16> result{};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t col = 0; col < 4; ++col) {
            float sum = 0.0F;
            for (std::size_t k = 0; k < 4; ++k) {
                sum += lhs.at((row * 4) + k) * rhs.at((k * 4) + col);
            }
            result.at((row * 4) + col) = sum;
        }
    }
    return result;
}

} // namespace detail

// The combined view-projection matrix mesh.vert.hlsl's new
// ViewProjectionUniform cbuffer actually carries -
// `output.Position = mul(ViewProjection, mul(Model, float4(Position,
// 1.0)))` needs exactly one matrix per draw call for the camera half of that
// composition, not two separately-pushed ones (this library's own "your
// call, document it" latitude - a single combined matrix, rather than
// separate view/projection uniforms, keeps the shader-side composition to
// one `mul` instead of two, and keeps this frame-constant value a single
// per-frame uniform push - see sdl3_mesh_pipeline.hpp's
// push_view_projection_uniform). Projection * View, never View * Projection
// - a world-space point is transformed into view space first, then
// projected, exactly the same order `mul(ViewProjection, mul(Model, ...))`
// already establishes for Model (innermost transform applied first). Not
// constexpr: composes to_projection_matrix, which itself is not constexpr
// (std::tan, see that function's own doc comment).
[[nodiscard]] inline std::array<float, 16> to_view_projection_matrix(const Camera& camera) noexcept {
    return detail::multiply_matrices(to_projection_matrix(camera), to_view_matrix(camera));
}

} // namespace atlas::render
