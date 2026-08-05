# atlas-render

**Status:** Seeded (issue #30), plus a formal backend contract and null backend (issue #148), plus a minimal
mesh/texture asset format and decoder (issue #152), plus the first real backend — SDL3/SDL_GPU window + device
bring-up (issue #151, the first slice of #69) — plus a real shader/pipeline/draw-call path proved with one
hardcoded triangle (issue #153) — plus real content: `Sdl3FrameBackend::submit()` resolves each
`DrawCommand`'s mesh/material through two GPU-upload caches, builds a model matrix from its `Transform`, and
issues one real indexed draw per surviving command (issue #154), superseding and removing issue #153's
hardcoded-triangle scaffolding entirely — plus genuine pixel-correctness proof and the
"every `FrameBackend` ships a smoke test" convention (issue #155): a real Vulkan software rasterizer
(Mesa's lavapipe/llvmpipe, apt package `mesa-vulkan-drivers`) genuinely renders in this project's own dev
sandbox once `SDL_HINT_VIDEO_DRIVER` is set to `"offscreen"` rather than `"dummy"` — see "Scoping decisions"
below for the full mechanism and why `"dummy"` could never have worked here regardless of GPU availability —
plus, now, real GPU-driven distance culling (issue #156, the actual point of #117/#148's design): a compute
pass decides visibility for every resolved `DrawCommand` at once and writes the result into an
indirect-argument buffer, and `submit()` issues one indirect draw call per resolved `DrawCommand` instead of
#154's unconditional direct ones — the CPU never decides visibility itself. Distance culling, not frustum
culling — no `Camera`/view-projection concept exists anywhere in Atlas yet, so true frustum culling has
nothing to test against; see "Scoping decisions" below for the full writeup, including the SDL
[#14754](https://github.com/libsdl-org/SDL/issues/14754) buffer-usage-flag analysis this design was built
around — plus, now, a real `Camera` type and its view-projection matrices (issue #181, closing the exact gap
#154/#156 both deliberately deferred): `atlas::render::Camera` (position/orientation plus FOV/aspect/near/far),
`to_view_matrix`/`to_projection_matrix`/`to_view_projection_matrix` (`include/atlas/render/camera.hpp`), a new
`ViewProjectionUniform` cbuffer in `mesh.vert.hlsl` composed against the existing model matrix, and
`Sdl3FrameBackend::submit()` now pushes the active camera's view-projection matrix alongside every draw's model
matrix — see "What's implemented" and "Scoping decisions" below for the full NDC-convention/matrix-math
writeup — plus, now, a skeleton resource format (issue #228, unblocking a sibling `atlas-rcc` `Animation` entry
that will reference one): `atlas::render::decode_skeleton` (`include/atlas/render/skeleton_asset.hpp`,
`src/skeleton_asset.cpp`) decodes a joint hierarchy plus each joint's bind-pose `Transform` from this library's
own minimal, hand-rolled binary format, mirroring `decode_mesh`/`decode_texture`'s exact shape — see "What's
implemented" and "Scoping decisions" below for the exact layout and the acyclic-by-construction hierarchy
invariant it enforces. Implements the `State → Renderer → Output` pattern (§19) for 3D
rendering: `atlas::render::build_frame` (`include/atlas/render/frame_builder.hpp`, `src/frame_builder.cpp`)
consumes composed `Transform`/`Renderable` property state — via the real `atlas::runtime::PropertyStore<T>`, not
a stub — for an explicitly ordered set of entities, and produces an `atlas::render::Frame`: an in-memory,
testable list of `DrawCommand`s (`include/atlas/render/frame.hpp`). The compile-time contract every backend
(real or null) must satisfy, the always-available `NullFrameBackend`, `Sdl3FrameBackend` — a real SDL3 window
plus an `SDL_GPU` device now drawing real per-`DrawCommand` content, GPU-culled, and genuinely camera-projected,
every frame — all exist; see "What's implemented" and "Scoping decisions" below for exactly what this round
does and doesn't do yet (camera *collision* — the next sub-issue, #182 — and true frustum-aware culling remain
open, see "Open questions").

## What's implemented

- **`atlas::core::Vec3`** and **`atlas::core::Quaternion`** (`atlas-core`'s
  `include/atlas/core/vec3.hpp`/`quaternion.hpp`) — plain 3D vector and rotation value types. Basic
  aggregates (rule of zero): no invariant either type enforces itself. These used to live in this library
  (`atlas::render::Vec3`/`Quaternion`) but were relocated to `atlas-core` so `atlas-physics` (issue #177)
  could use the exact same representation for body positions/orientations without depending on
  `atlas-render` — `atlas-physics` is not client/presentation-only the way this library is (a headless
  server composes it too), so it must sit below both, not depend on either.
- **`atlas::render::Transform`** (`include/atlas/render/transform.hpp`) — a renderable entity's full spatial state: `position`,
  `rotation`, `scale` (defaulting to unit scale). A basic aggregate.
- **`atlas::render::lerp`** (three overloads: `float`, `core::Vec3`, `Transform`) and
  **`atlas::render::nlerp`** (`core::Quaternion`) — presentation-only interpolation (spec §4: "Wall-clock time may be used only for
  presentation-only concerns (audio/render interpolation)... must never feed back into simulation state").
  `alpha` is always a caller-supplied `double`; none of these functions read a clock of any kind, deterministic
  or otherwise — see Scoping decisions below for why rotation interpolation is `nlerp`, never `slerp`.
- **`atlas::render::Renderable`** (`include/atlas/render/renderable.hpp`) — a renderable entity's resource
  references: `mesh` and `material`, both `atlas::ResourceId` (`atlas-resource`, identity only — see that
  library's own scoping note). A basic aggregate.
- **`atlas::render::DrawCommand`** and **`atlas::render::Frame`** (`include/atlas/render/frame.hpp`) — a
  single resolved draw instruction (`entity`, `transform`, `mesh`, `material`), and the output of one
  render pass: `tick` (`atlas::core::Time`, carried through unmodified — never read back) plus an ordered
  `std::vector<DrawCommand>`. Both basic aggregates.
- **`atlas::render::build_frame`** (`include/atlas/render/frame_builder.hpp`) — the actual State → Renderer
  → Output function. Given a `std::span<const EntityRef>`, a `PropertyStore<Transform>`, a
  `PropertyStore<Renderable>`, and a `Time` tick, it visits entities in exactly the caller-supplied order
  and emits one `DrawCommand` per entity that has both a stored `Transform` and a stored `Renderable` whose
  `mesh` and `material` are both non-null — skipping (never substituting or coercing) every other case.
- **`atlas::render::FrameBackend`** (`include/atlas/render/frame_backend.hpp`, issue #148) — the compile-time
  contract (a C++ `concept`, checked via `static_assert` like every generated contract in this project — spec
  §5: "never a runtime interface table or virtual dispatch lookup") every backend, real or null, must satisfy:
  `submit(const Frame&) -> void` and `last_completed_tick() -> std::optional<core::Time>`. A conforming backend
  must always receive the *complete* `Frame` `build_frame` produced — culling is entirely the backend's own
  concern (issue #117, resolved), specifically so a real backend can do GPU-driven visibility (compute-shader
  culling into an indirect-draw buffer) instead of being forced through CPU-side visibility decisions made
  upstream. `last_completed_tick()` reports the tick of the most recent `Frame` the backend has actually
  *finished* presenting (not merely accepted) — lets a caller do its own backpressure/batching and gives
  frame-drop diagnostics / an FPS meter for free by sampling it over time, without Atlas ever imposing a
  batching policy.
- **`atlas::render::NullFrameBackend`** (`include/atlas/render/null_frame_backend.hpp`, issue #148) — the
  always-buildable `FrameBackend`: does nothing with a `Frame`'s draw commands, zero third-party dependencies.
  Since it performs no real presentation work, the tick it last accepted is "instantly complete" — no GPU
  fence to wait on — so `last_completed_tick()` just reports whatever tick `submit()` was last called with,
  and `std::nullopt` before any call at all.
- **`atlas::render::decode_mesh`** (`include/atlas/render/mesh_asset.hpp`, `src/mesh_asset.cpp`, issue
  #152) — decodes raw bytes (the kind `atlas::resource::ResourceRegistry::resolve()` produces) against this
  library's own minimal, hand-rolled binary mesh format into a GPU-upload-ready `atlas::render::DecodedMesh`
  (`std::vector<Vertex>` + `std::vector<std::uint32_t>` indices, where `Vertex` is a position/normal/UV triple
  of plain floats, reusing `atlas::core::Vec3`). Returns `std::optional<DecodedMesh>` —
  `std::nullopt` for any malformed or truncated input — rather than throwing; see "Scoping decisions" below
  for the exact format layout and why it needs no third-party dependency.
- **`atlas::render::decode_texture`** (`include/atlas/render/texture_asset.hpp`, `src/texture_asset.cpp`,
  issue #152) — decodes raw bytes the same way, against this library's own minimal, hand-rolled raw/
  uncompressed RGBA8 texture format, into a GPU-upload-ready `atlas::render::DecodedTexture` (pixel bytes plus
  `width`/`height`). Also `std::optional`-returning, including an explicit overflow-safe rejection of
  adversarial width/height values rather than a naive multiplication that could wrap around — see "Scoping
  decisions" below.
- **`atlas::render::decode_skeleton`** (`include/atlas/render/skeleton_asset.hpp`, `src/skeleton_asset.cpp`,
  issue #228) — decodes raw bytes the same way, against this library's own minimal, hand-rolled skeleton
  format, into an `atlas::render::SkeletonAsset`: a flat `std::vector<Joint>`, each `Joint` a `parent_index`
  (`no_parent_joint` for a root) plus a bind-pose `atlas::render::Transform` — reusing `transform.hpp`'s
  existing type verbatim rather than introducing a second, parallel transform representation for skeletal
  data. `std::optional`-returning, `std::nullopt` for any malformed/truncated input, including a joint whose
  `parent_index` is neither `no_parent_joint` nor strictly less than its own array index — see "Scoping
  decisions" below for the exact layout and why that ordering requirement is the entire cycle/range validation
  this format needs. A `joint_count` of zero decodes to a well-formed, empty `SkeletonAsset`, mirroring
  `decode_mesh`'s own zero-vertex/zero-index stance. Vertex-to-joint binding (skinning weights), GPU skinning,
  and retargeting between mismatched skeletons are all explicitly out of scope for this issue — see "Open
  questions" below.
- **`atlas::render::to_model_matrix`** (`include/atlas/render/transform.hpp`, issue #154) — builds a row-major
  4×4 model matrix (translation × rotation × scale, standard TRS composition for a column-vector transform)
  from a `Transform`. Pure C++, no SDL/GPU dependency, always compiled (both `NULL` and `SDL3` backends) and
  directly unit-tested (`tests/atlas-render/transform_test.cpp`, `ToModelMatrix.*`) — the one piece of this
  round's math independently verifiable without a real GPU. See "Scoping decisions" below for why this is the
  entire "camera" this round has: no view-projection concept exists anywhere in Atlas yet, so the model matrix
  is pushed to the vertex shader alone.
- **`atlas::render::MeshUploadCache`** and **`atlas::render::TextureUploadCache`**
  (`include/atlas/render/mesh_upload_cache.hpp`/`texture_upload_cache.hpp`,
  `src/mesh_upload_cache.cpp`/`texture_upload_cache.cpp`, issue #154, only compiled when
  `ATLAS_RENDER_BACKEND=SDL3`) — two focused caches, each mirroring `atlas-audio`'s `DecodeCache` (issue #166)
  exactly for the resolve/decode half (`ResourceRegistry::resolve()` → `decode_mesh`/`decode_texture`, both
  success and every failure mode cached so a resource that fails once is never retried every frame), extended
  with the one additional step audio's CPU-only cache doesn't need: uploading the decoded data to real
  `SDL_GPUBuffer`s (mesh: vertex + index buffer) or an `SDL_GPUTexture` (texture), needing a live
  `SDL_GPUDevice*` supplied at construction alongside `registry`/`type_name`. `get_or_upload(ResourceId)`
  returns a `const MeshUploadResult&`/`const TextureUploadResult&` — payload fields (buffers/texture,
  `index_count`, `width`/`height`) meaningful only when `status == Ok`, mirroring `DecodeCacheResult`'s own
  convention. GPU teardown is explicit (`release()`), never these classes' own destructors — see their header
  comments for the same `Sdl3FrameBackend`-destroy()-ordering hazard `Sdl3MeshPipeline` (below) also has to
  avoid. Type names follow the established convention confirmed in
  `tests/atlas-resource/resource_registry_test.cpp`: `"Mesh"` and `"Texture"`.
- **`atlas::render::Sdl3FrameBackend`** (`include/atlas/render/sdl3_frame_backend.hpp`,
  `src/sdl3_frame_backend.cpp`, issue #151, only compiled when `ATLAS_RENDER_BACKEND=SDL3`) — the first real
  (non-null) `FrameBackend`: constructs an SDL3 window and an `SDL_GPU` device, and claims the window for that
  device (swapchain setup). Construction now also takes a `const atlas::resource::ResourceRegistry&` (issue
  #154 — see "Scoping decisions" below for why an added constructor parameter, not some other threading
  mechanism), used to construct the two upload caches above; `registry` must outlive the backend, mirroring
  `atlas-audio::DecodeCache`'s own "registry must outlive this cache" contract. Construction also now takes a
  `DistanceCullConfig distance_cull = {}` (issue #156 — reference point + max distance, defaulting to the
  origin and a generous 1000 units) used to build the real distance-cull compute pipeline alongside the mesh
  one. `submit(const Frame&)` acquires the swapchain texture, clears it, then — issue #154's original point,
  extended by issue #156 — resolves each `DrawCommand`'s `mesh`/`material` through the two caches, skips (never
  substitutes) any command whose mesh or texture failed to resolve/decode/upload, and builds a model matrix
  from its `Transform` (`to_model_matrix`, above); every surviving command's position and resolved
  `index_count` then feeds one real compute-shader distance-cull pass
  (`dispatch_sdl3_distance_cull`, `sdl3_distance_cull_pipeline.hpp`) that decides visibility for the *entire*
  set at once, before `submit()` issues one `draw_sdl3_mesh_pipeline_indirect` call per surviving command
  (`draw_count=1`, reading that command's own compute-decided entry), in `Frame::draw_commands`'s own order —
  then presents. The CPU still iterates every resolved command to bind its own (already-resolved) mesh/texture
  (an unavoidable consequence of one vertex/index buffer per mesh, `MeshUploadCache`, issue #154 — see
  "Scoping decisions" below), but never branches on distance itself; that decision lives entirely in the
  compute-written indirect buffer. `last_completed_tick()` is driven by a real
  `SDL_GPUFence` per submission, polled and released as fences signal — never `NullFrameBackend`'s "instantly
  complete" shortcut. An encapsulated class (not a basic aggregate, unlike this library's other types): it owns
  real OS/GPU resources (window, device, mesh pipeline, distance-cull compute pipeline, both upload caches,
  in-flight fences) with a genuine invariant to protect (every acquired handle released exactly once, in the
  right order), the same exception to Rule of Zero CLAUDE.md carves out for `atlas::entity::EntityRegistry`.
  Construction can fail (no GPU/display hardware, no supported `SDL_GPU` backend, a shader/pipeline build
  failure — the common case on CI runners) and reports that by throwing `std::runtime_error`, per CLAUDE.md's
  documented `std::expected` incompatibility — see "Scoping decisions" below (the headless-CI paragraph) for
  how this library's own tests handle that. **Issue #174**: a second, alternate constructor takes an
  `atlas::windowing::Sdl3SharedWindow&` instead of `window_title`/`width`/`height`/`extra_window_flags` —
  claims that already-created window for this backend's own `SDL_GPU` device rather than creating (and later
  destroying) its own, so a real host running this backend alongside a real
  `atlas::input::Sdl3RawSignalSource` presents into the exact same window that backend reads real OS
  keyboard/mouse focus from, rather than each library silently owning its own hidden window. The
  self-contained constructor above is unchanged and remains the right choice for a host that only wants
  rendering. See `libraries/atlas-windowing/README.md` for the full rationale and why this couldn't be solved
  by a direct dependency between `atlas-render` and `atlas-input` instead.
- **`atlas::render::Sdl3MeshPipeline`** (`include/atlas/render/sdl3_mesh_pipeline.hpp`,
  `src/sdl3_mesh_pipeline.cpp`, issue #154, only compiled when `ATLAS_RENDER_BACKEND=SDL3`) — the real mesh
  pipeline superseding and replacing issue #153's `Sdl3TrianglePipeline` (deleted as part of this issue, along
  with its two HLSL shaders, its own test file, and its construction/draw/teardown wiring inside
  `Sdl3FrameBackend` — #153's own scaffolding was explicit about being superseded once real content arrived).
  `create_sdl3_mesh_pipeline(device, swapchain_format)` compiles the checked-in `shaders/mesh.{vert,frag}.hlsl`
  (the same `SDL_shadercross` HLSL→SPIRV→`SDL_GPUShader` sequence #153 established), builds a
  `SDL_GPUGraphicsPipeline` whose vertex input state matches `decode_mesh`'s `Vertex` layout exactly
  (`Position`@0/`Normal`@1/`UV`@2 — `Normal` is declared even though `mesh.frag.hlsl` never reads it, see that
  shader's own doc comment for why), and creates one shared linear-filtering, clamp-to-edge `SDL_GPUSampler`
  (texture-sampling *state* has no per-resource variation yet — see "Open questions"). `draw_sdl3_mesh_pipeline`
  binds the pipeline, pushes the model matrix as a vertex uniform (`SDL_PushGPUVertexUniformData`, verified
  against the real fetched `SDL_gpu.h`), binds the resolved mesh's vertex/index buffers and the resolved
  texture (paired with the shared sampler), and issues one `SDL_DrawGPUIndexedPrimitives` call —
  `decode_mesh` produces indices, and this function always uses them. Issue #156 adds a sibling,
  `draw_sdl3_mesh_pipeline_indirect` — the identical bind sequence (factored into a shared, file-local
  `bind_sdl3_mesh_pipeline` helper), but issuing `SDL_DrawGPUIndexedPrimitivesIndirect` (`draw_count=1`,
  reading one caller-supplied buffer offset) instead — `Sdl3FrameBackend::submit()` now uses only the indirect
  variant; the direct `draw_sdl3_mesh_pipeline` remains, still exercised directly by
  `sdl3_pixel_correctness_test.cpp`'s original (non-culling) quadrant test. `destroy_sdl3_mesh_pipeline()`
  releases both handles and is idempotent, managed explicitly by `Sdl3FrameBackend::destroy()` rather than its
  own RAII class, mirroring `Sdl3TrianglePipeline`'s own now-removed lifetime-ordering discipline.
- **`atlas::render::Sdl3DistanceCullPipeline`** (`include/atlas/render/sdl3_distance_cull_pipeline.hpp`,
  `src/sdl3_distance_cull_pipeline.cpp`, `shaders/distance_cull.comp.hlsl`, issue #156, only compiled when
  `ATLAS_RENDER_BACKEND=SDL3`) — the real GPU-driven distance-culling compute pipeline that is this issue's
  entire point. `create_sdl3_distance_cull_pipeline(device)` compiles the checked-in compute HLSL via
  `SDL_shadercross`'s compute-specific sequence — `SDL_ShaderCross_CompileSPIRVFromHLSL` (stage
  `SDL_SHADERCROSS_SHADERSTAGE_COMPUTE`) → `SDL_ShaderCross_ReflectComputeSPIRV` →
  `SDL_ShaderCross_CompileComputePipelineFromSPIRV` — verified against the real fetched
  `SDL3_shadercross/SDL_shadercross.h` to be a distinct, parallel three-call sequence to the graphics-shader one
  `sdl3_mesh_pipeline.cpp` already established (compute has its own reflection/compile entry points, not a mode
  of the graphics ones — confirmed by reading the header, not assumed). `dispatch_sdl3_distance_cull(command_buffer,
  device, pipeline, objects, config)` uploads `objects` (one `DistanceCullObjectInput` — position + the
  resolved mesh's own `index_count` — per surviving-so-far `DrawCommand`) to a freshly created
  `SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ` buffer via a transfer buffer and a copy pass on the caller's own
  command buffer, runs one compute pass dispatching `ceil(objects.size() / 64)` workgroups (the shader's own
  `[numthreads(64, 1, 1)]`; the shader itself bounds-checks past `objects.size()` via
  `StructuredBuffer::GetDimensions()`, not a separately-passed count that could fall out of sync), and returns
  a freshly created `SDL_GPU_BUFFERUSAGE_INDIRECT | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE` buffer (**never**
  combined with `SDL_GPU_BUFFERUSAGE_INDEX` — see "Scoping decisions" below for the full SDL #14754 analysis
  this exact combination satisfies) holding one tightly-packed `SDL_GPUIndexedIndirectDrawCommand`-shaped entry
  per object, `num_instances` 1 or 0 depending on whether that object's position lies within `config.max_distance`
  of `config.reference_point`. Every transient GPU resource this function allocates is returned via
  `Sdl3DistanceCullTransients` and is only safe to release (`release_sdl3_distance_cull_transients`) once the
  command buffer that both wrote and read them has actually been submitted — never before — generalizing
  `mesh_upload_cache.cpp`'s own "safe to release once the copy is submitted" precedent from a transfer buffer
  specifically to every SDL_GPU resource this function creates. Throws `std::runtime_error` on any GPU call
  failure, matching every other SDL3-backend construction/dispatch failure in this library.
- **`atlas::render::Camera`** (`include/atlas/render/camera.hpp`, issue #181) — the real `Camera`/view-projection
  concept #154/#156 both deliberately deferred, finally closed. A basic aggregate (rule of zero), mirroring
  `Transform`'s own stance: `position` (`core::Vec3`) and `orientation` (`core::Quaternion`) for the view side
  (the exact same representation `Transform` uses, minus `scale` — a camera is never scaled), plus
  `vertical_fov_radians` (radians, not degrees — this library's own established convention throughout),
  `aspect_ratio`, `near_clip`, `far_clip` for the projection side. Defaults describe a generic, immediately-usable
  camera: world origin, identity orientation, a 60-degree vertical FOV, 16:9 aspect, a 0.1-to-1000-unit near/far
  range. Presentation-only per §4/§20 ("view/projection... presentation concern") — never feeds back into
  simulation state, the same carve-out `Transform`'s own `lerp`/`nlerp` interpolation already documents.
- **`atlas::render::to_view_matrix`**, **`to_projection_matrix`**, and **`to_view_projection_matrix`**
  (`include/atlas/render/camera.hpp`, issue #181) — pure, `constexpr`/`inline` functions, no GPU/SDL dependency,
  mirroring `to_model_matrix`'s own "math lives in a header, GPU wiring is separate" split exactly, and reusing
  its exact quaternion-to-rotation-matrix formula (factored out as `transform.hpp`'s own
  `detail::to_rotation_matrix`, shared rather than re-derived) for the view matrix's rotation block. `to_view_matrix`
  builds the inverse of the camera's own world transform (`R^T` composed with `-R^T * position`, never position
  and orientation negated independently — see "Scoping decisions" below for the full derivation and the bug that
  shortcut would introduce). `to_projection_matrix` targets SDL_GPU's own documented left-handed, `[0, 1]`-depth
  NDC convention exactly (verified against the real fetched `SDL_gpu.h`, not assumed) with the camera's forward
  axis as its own local +Z (matching that convention's own D3D12/Metal handedness end to end, so `clip.w == view.z`
  needs no sign flip). `to_view_projection_matrix` composes `Projection * View` into the single combined matrix
  `mesh.vert.hlsl`'s new cbuffer actually carries. Same row-major `std::array<float, 16>` layout `to_model_matrix`
  already established throughout — see "Scoping decisions" below for the full math and the real SDL_gpu.h
  verification.
- **`Sdl3FrameBackend` now owns an active `Camera`** (issue #181) — both constructors gain a trailing
  `Camera camera = {}` parameter (defaulting to `Camera{}`'s own generic default, so every pre-existing call site
  compiles unchanged), and a new `set_camera(const Camera&)`/`camera()` pair lets a caller update it every tick
  (unlike `distance_cull`, a real camera moves constantly, so this is a settable method, not construction-only
  state). `submit()` now pushes the active camera's combined view-projection matrix
  (`push_view_projection_uniform`, `sdl3_mesh_pipeline.hpp`) once per command buffer — before the per-`DrawCommand`
  draw loop, never re-pushed per draw the way the model matrix is, since SDL_GPU's own documented uniform-slot
  semantics keep a pushed value live across the whole command buffer until pushed again — alongside every
  surviving `DrawCommand`'s own model matrix, exactly as issue #181 requires.

## Scoping decisions

**Issue #156: SDL #14754 re-verified directly against this project's own vendored SDL3 source before writing
any buffer-creation code — proceeding unpatched was the right call, confirmed independently, not just trusted
from the issue's own summary.** [SDL #14754](https://github.com/libsdl-org/SDL/issues/14754) ("GPU: Vulkan
missing/incorrect barrier for INDIRECT argument buffer written by compute then consumed by
vkCmdDrawIndexedIndirect") is real and still open. Reading this project's own fetched
`_deps/sdl3-src/src/gpu/vulkan/SDL_gpu_vulkan.c` directly: `VULKAN_INTERNAL_DefaultBufferUsageMode()` picks a
buffer's default Vulkan sync/barrier state from its creation flags in a fixed, explicitly-commented priority
order (`// NOTE: order matters here!`) — `SDL_GPU_BUFFERUSAGE_VERTEX`, then `..._INDEX`, then `..._INDIRECT`,
then the storage-read/write variants. A buffer created with **both** `..._INDEX` and `..._INDIRECT` set (the
SDL issue's own exact repro) resolves its *default* state to `INDEX_READ`, not `INDIRECT` — the wrong barrier
after a compute write, exactly the reported bug. This library's own indirect buffer
(`sdl3_distance_cull_pipeline.cpp`) is created with **only** `SDL_GPU_BUFFERUSAGE_INDIRECT |
SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE` — no `VERTEX`, no `INDEX` — so it resolves its default state to
`INDIRECT` correctly (the real per-mesh index data lives in each mesh's own entirely separate index buffer,
`MeshUploadCache`, issue #154, `SDL_GPU_BUFFERUSAGE_INDEX` only, never touching this buffer). Also traced
`VULKAN_INTERNAL_BufferMemoryBarrier`'s own handling of both directions of the
`COMPUTE_STORAGE_READ_WRITE`↔`INDIRECT` transition (the exact one this design's compute-write-then-indirect-
read sequence produces): both directions map to correct, distinct Vulkan pipeline stage/access-mask pairs
(`VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT`/`VK_ACCESS_SHADER_WRITE_BIT` → `VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT`/
`VK_ACCESS_INDIRECT_COMMAND_READ_BIT`) — correct end-to-end for this design's actual usage pattern. **No patch
against vendored SDL3 was applied or is needed.** If a future round ever needs to combine `INDEX` and
`INDIRECT` on one buffer (e.g. real batched mesh-merging, see below), this analysis must be re-verified against
the real source again — it is scoped to the specific flag combination this issue's design actually uses, not a
blanket "the bug doesn't matter" conclusion.

**Distance culling, not frustum culling — a second gap found and resolved the same way issue #154 handled its
own Camera gap.** "Frustum culling" as this issue was originally scoped assumes a camera/view-projection to
test bounds against — none exists anywhere in Atlas yet (the same gap #154 already flagged and deferred, see
`transform.hpp`'s `to_model_matrix` doc comment and "Open questions" below). Revised scope: cull any
`DrawCommand` whose `Transform.position` exceeds a fixed maximum distance from a reference point
(`DistanceCullConfig`, `sdl3_distance_cull_pipeline.hpp`) — a real, legitimate GPU culling technique in its own
right that needs no camera at all, proving the actual compute → indirect-draw mechanism this issue exists for
without faking frustum math against a view-projection that doesn't exist. True frustum/camera-aware culling
remains a distinct, later follow-up once a real `Camera` type lands (flagged again below under "Open
questions," not silently precluded by this round's choice).

**One `draw_count=1` indirect draw call per resolved `DrawCommand`, not one `draw_count=N` call spanning every
`DrawCommand` in the Frame — a direct consequence of this library's existing one-vertex/index-buffer-per-mesh
architecture (`MeshUploadCache`, issue #154), not an arbitrary simplification.** `SDL_DrawGPUIndexedPrimitivesIndirect`
reads its draw parameters from a buffer, but the vertex/index buffers it draws *from* are still whatever was
bound before the call — a single indirect call spanning multiple entries can only span entries that share one
bound vertex/index buffer. Since each mesh has its own separate buffer pair, batching heterogeneous meshes into
one shared buffer for a true single `draw_count=N` multi-draw call would be a distinct, unrequested mesh-batching
redesign this issue does not attempt (flagged below under "Open questions" as real, unaddressed follow-up work).
This round's mechanism instead applies the issue's own suggested "always request the maximum possible draw
count, with the compute shader zeroing non-surviving entries' `instance_count` to 0" idea at the granularity
this architecture actually supports: **every** resolved `DrawCommand` gets its own indirect draw call with a
statically-known `draw_count=1`, reading one buffer entry whose `num_instances` (computed by one shared compute
dispatch covering the whole frame at once) is 1 or 0 — a 0-instance indirect draw call issues no vertex/geometry
work and costs negligible GPU time (verified empirically, not assumed — see the pixel-correctness test below),
which is what lets the CPU always issue the call unconditionally rather than ever branching on visibility
itself. The "communicate how many objects survived back to the CPU" problem the issue itself raises (SDL_GPU's
indirect draw needing a fixed `draw_count` known ahead of time) simply doesn't arise at this granularity: every
call's own `draw_count` is always exactly 1, known unconditionally, regardless of that entry's own cull outcome.

**The distance-cull compute pass's input/output GPU buffers are allocated fresh every `submit()` call, sized
exactly to that frame's own resolved-`DrawCommand` count, and released immediately after that frame's command
buffer is submitted — no persistent, growable buffer pool.** `Frame::draw_commands` has no compile-time bound,
so "sized for the maximum possible object count in a Frame" (the issue's own wording) is satisfied by always
allocating exactly what the current frame needs, computed on the CPU before any GPU work begins, rather than a
separately-tracked "largest frame ever seen" capacity that would need careful fence-synchronized regrowth to
avoid releasing a buffer still referenced by in-flight GPU work from a prior frame. `mesh_upload_cache.cpp`'s
own `upload_gpu_buffer` already established that a submitted command buffer's resources are safe to release
immediately after (SDL_GPU itself defers actual reclamation) — `dispatch_sdl3_distance_cull`'s own transient
resources (input storage buffer, its transfer buffer, the indirect buffer) generalize that same discipline. The
real cost this trades away is allocation churn (a fresh `SDL_CreateGPUBuffer`/`SDL_CreateGPUTransferBuffer` pair
every frame, rather than reusing one long-lived pair) — an acceptable trade for "prove the mechanism, not
production-ready," this library's own established bar elsewhere (e.g. HLSL compiled at runtime, not offline) —
flagged below under "Open questions" as real, unaddressed follow-up for a production render loop.

**Issue #181 closes the `Camera`/view-projection gap #154/#156 both deliberately deferred — the writeup below is
the full matrix-math/NDC-convention/shader-layout derivation, verified rather than assumed at every step.**

**View matrix derivation: `R^T` composed with `-R^T * position`, never position and orientation negated
independently.** For a camera's world transform `world = Translate(position) * Rotate(orientation)` (i.e.
`world_point = R * local_point + position`), the inverse mapping a world-space point into the camera's own
view space is `view_point = R^-1 * (world_point - position)`. `R` is orthonormal for a unit quaternion (the same
assumption `to_model_matrix`/`to_rotation_matrix` already make), so `R^-1 == R^T` — expanding gives
`view_point = R^T * world_point - R^T * position`, exactly `Transform`-shaped (a rotation block plus a
translation column) but with the rotation block transposed and the translation column computed from
`-R^T * position` rather than taken directly from `position`. A common, subtly wrong shortcut is negating
`position` and `orientation` independently (e.g. building a "camera transform" with `-position` and the inverse
quaternion, then running it through `to_model_matrix` unchanged) — this produces the *wrong* matrix the moment
the camera is both rotated and translated, since `to_model_matrix`'s own translation column assumes the
un-transposed rotation basis. `to_view_matrix` (`camera.hpp`) reuses `transform.hpp`'s own
`detail::to_rotation_matrix` (factored out specifically so both functions share one quaternion-to-rotation-matrix
formula, never two independently-derived ones that could silently disagree in a rounding corner case), transposes
it by construction (reading `R`'s columns as `R^T`'s rows rather than computing `R` then transposing it as a
separate step), and computes the translation column from `-R^T * position` directly. Verified with a test that a
"negate independently" bug would fail (`camera_test.cpp`,
`ToViewMatrix.RotatedAndTranslatedCameraMapsItsOwnForwardAxisPointToPositiveViewSpaceZ`): a camera rotated 90
degrees about Y and translated away from the origin must still map a world point lying along its own forward axis
to positive view-space Z — a test written and confirmed to fail under the "negate independently" formulation
before the correct one was committed, not merely a test that happens to pass.

**Projection matrix derivation and NDC convention: left-handed, `[0, 1]` depth, camera forward == local +Z —
verified against the real fetched `SDL_gpu.h`, not assumed.** That header's own "Coordinate System" section
states: "a left-handed coordinate system, following the convention of D3D12 and Metal... Z values range from
`[0.0, 1.0]` where 0 is the near plane." Reusing D3D12's own convention end to end (camera forward == its own
local +Z axis, matching `to_view_matrix`'s identity-orientation basis) avoids introducing a handedness flip
anywhere between view and clip space — the simplest choice directly consistent with what SDL_GPU itself already
documents needing. The resulting matrix (column-vector convention, `clip = P * view_point`, matching
`mesh.vert.hlsl`'s `mul(matrix, vector)` call):

```
xScale = 1 / (aspect_ratio * tan(vertical_fov_radians / 2))
yScale = 1 / tan(vertical_fov_radians / 2)
clip.x = xScale * view.x
clip.y = yScale * view.y
clip.z = (far_clip / (far_clip - near_clip)) * view.z - (near_clip * far_clip / (far_clip - near_clip))
clip.w = view.z
```

This is the *transpose* of D3D's traditionally-documented `XMMatrixPerspectiveFovLH` formula (which presents its
matrix for the row-vector convention `view_point * M`) — independently re-derived and verified here, not copied,
by checking the resulting NDC depth at `z_view == near_clip` and `z_view == far_clip` lands at exactly `0` and `1`
respectively (`camera_test.cpp`, `ToProjectionMatrix.NearPlanePointProjectsToNdcZOfZero`/`FarPlanePointProjectsToNdcZOfOne`),
and that a 90-degree vertical FOV with a 1:1 aspect ratio (`tan(45°) == 1`) maps the frustum edge to NDC `±1`
exactly (`ToProjectionMatrix.NinetyDegreeFovWithUnitAspectMapsTheFrustumEdgeToNdcUnity`). `clip.w == view.z`
(never `-view.z`, the RH/OpenGL convention's usual construction) is the direct consequence of choosing
forward == +Z: a visible point already has a positive view-space Z, so it already has a positive `w` without
needing a sign flip — verified directly (`ToProjectionMatrix.ClipSpaceWEqualsViewSpaceZ`). `to_projection_matrix`
is `inline`, not `constexpr`, unlike `to_view_matrix`/`to_model_matrix`: `std::tan` is not a core constant
expression in C++23, the same "correctly-rounded but not a constant expression" situation `transform.hpp`'s own
`nlerp` already documents for `std::sqrt` — this project's established answer is `inline`, not `constexpr`, for
exactly that case.

**A single combined `to_view_projection_matrix` (`Projection * View`), not two separately-pushed uniforms — this
library's own "your call, document it" latitude, resolved in favor of the simpler shader-side composition.**
`mesh.vert.hlsl`'s `output.Position = mul(ViewProjection, mul(Model, float4(Position, 1.0)))` needs exactly one
matrix per draw call for the camera half of that composition — a single combined matrix keeps the shader to one
`mul` instead of two, and keeps the camera's own contribution a single per-frame uniform push
(`push_view_projection_uniform`, below) rather than two. `to_view_projection_matrix` is not `constexpr` for the
same reason `to_projection_matrix` isn't (composes it internally).

**Shader cbuffer layout: `ViewProjectionUniform` at `b1, space1` — verified against the real fetched `SDL_gpu.h`'s
own documented SPIR-V resource-set convention, not guessed.** That header states, for vertex-stage uniform
buffers: `(b[n], space1)`; for fragment-stage sampled textures/samplers: `(t[n]/s[n], space2)`; for fragment-stage
uniform buffers: `(b[n], space3)`. `mesh.vert.hlsl`'s existing `ModelUniform` already occupies `b0, space1` (the
first vertex-stage uniform buffer slot) — the new view-projection uniform is the *next* vertex-stage uniform
buffer slot, `b1, space1`, **not** `b0, space2` (a plausible-looking but wrong guess this issue's own tracking
explicitly flagged as a trap): `space2` is the fragment stage's own texture/sampler space, already occupied by
`mesh.frag.hlsl`'s `AlbedoTexture`/`AlbedoSampler` (`t0, s0, space2`) — reusing it for a vertex-stage uniform
buffer would collide with the documented convention itself, not merely with an already-used register. Pushed via
`SDL_PushGPUVertexUniformData(command_buffer, /*slot_index=*/1, ...)` — slot 1 is the second vertex-stage uniform
buffer slot, matching `b1`'s position immediately after `ModelUniform`'s own `b0`/slot 0.

**`push_view_projection_uniform` is called once per command buffer, never once per draw the way the model matrix
is — a direct, intentional use of SDL_GPU's own documented uniform-slot semantics, not an oversight.** The real
fetched `SDL_gpu.h` states: "Uniform data pushed to a slot on a stage keeps its value throughout the command
buffer until you call the relevant Push function on that slot again." The view-projection matrix is identical for
every draw in a frame (one active `Camera`, `Sdl3FrameBackend::submit()`), unlike the model matrix (different per
`DrawCommand`) — so `submit()` pushes it exactly once, before the per-`DrawCommand` draw loop, rather than
re-pushing an unchanged value alongside every draw's own model-matrix push.

**Single active camera per `Sdl3FrameBackend`, no multiple viewports/split-screen — explicitly out of scope, per
this issue's own scope boundary, not attempted.** `camera_` is one plain member, replaced wholesale by
`set_camera()`; nothing in `submit()` iterates more than one camera or renders more than one color target per
call.

**Real pixel-level proof, not "doesn't throw" — mirrors issue #155's own established bar exactly.**
`sdl3_pixel_correctness_test.cpp` gains three camera-specific tests reusing that file's own off-window
texture-readback technique: a quad placed at a known world distance, with the camera's FOV/aspect chosen so the
projected quad's NDC extent is exactly, analytically predictable (`ndc.x == world.x - camera.position.x` for this
specific setup, independently re-derived and documented in that file directly) —
`CameraLookingDirectlyAtTheObjectProducesTheExactCheckerTexturePixelsPerQuadrant` (the camera reproduces the exact
same quadrant colors the pre-existing no-camera test already established),
`CameraPointedAwayFromTheObjectProducesOnlyClearColorPixels` (moving the camera far to the side pushes the object
entirely out of frame — every sampled pixel is the clear color, not merely "different"), and the stronger proof
issue #181 itself suggested, `MovingTheCameraShiftsTheObjectsScreenSpacePosition` (two fixed screen-space sample
points flip coverage — one from covered to clear, the other from clear to covered — in the exact direction a real
camera move analytically predicts, not merely appearing/disappearing independently).

**Two upload caches (`MeshUploadCache`, `TextureUploadCache`), not one combined cache class.** Mirrors
`atlas-audio::DecodeCache` (issue #166) for the resolve/decode half exactly, per issue #154's own locked-in
scope, but mesh and texture data have different GPU-upload shapes (vertex+index buffer pair vs. a single
texture) and different decode functions (`decode_mesh` vs. `decode_texture`) — a single over-generalized cache
templated or branching over "which kind of asset" would need more abstraction than either of the exactly two
call sites (`Sdl3FrameBackend`'s own mesh/material resolution in `submit()`) actually needs, the same
"don't add abstraction beyond what's needed" principle CLAUDE.md states for architecture generally.

**`Sdl3FrameBackend`'s constructor gained a `const atlas::resource::ResourceRegistry&` parameter — the obvious
threading mechanism, not a special-cased setter or a global.** `ResourceRegistry` is constructed once at host
startup and outlives every backend composed against it in every realistic composition, the same "outlives"
contract `atlas-audio::DecodeCache` already documents for its own `registry` parameter — extending that same
contract to `Sdl3FrameBackend` (which now owns two caches with an identical contract) needed no new pattern,
just a new constructor argument threaded through to both caches at construction time.

**The mesh/fragment shader's HLSL resource bindings (`register(b0, space1)` for the vertex uniform,
`register(t0, space2)`/`register(s0, space2)` for the fragment texture/sampler pair) were verified against a
real standalone `SDL_ShaderCross_CompileSPIRVFromHLSL` + `SDL_ShaderCross_ReflectGraphicsSPIRV` compile/reflect
run before being committed, not assumed from `SDL_CreateGPUShader`'s documented SPIR-V resource-set convention
alone.** That convention (vertex stage: set 0 textures/samplers, set 1 uniform buffers; fragment stage: set 2
textures/samplers, set 3 uniform buffers) is stated in the real fetched `SDL_gpu.h`, but nothing in this
project's own `SDL_shadercross` invocation (`SDL_ShaderCross_CompileSPIRVFromHLSL`, no `-fvk-*-shift` arguments
passed — confirmed by reading `SDL_shadercross`'s own `SDL_shadercross.c` at the pinned commit) shifts HLSL
`register(space)` annotations automatically; DXC's default SPIR-V codegen maps `spaceN` directly to descriptor
set `N` only when the HLSL source states it explicitly. The reflected output confirmed `num_uniform_buffers=1`
or `num_samplers=1` (as appropriate) on each stage and the expected `TEXCOORD0`/`TEXCOORD2` input locations for
`mesh.vert.hlsl` (`TEXCOORD1`/`Normal` is correctly elided from the reflected input list since the shader body
never reads it, matching D3D12-style dead-code elimination — SPIRV-Cross's reflection numbers surviving inputs
by their original semantic index, not a post-elimination sequential position, so the pipeline's own vertex
input state locations still line up).

**Backend selection is a CMake configure-time choice (`ATLAS_RENDER_BACKEND`, default `NULL`), never a runtime
factory or plugin lookup (spec §4).** `"NULL"` and `"SDL3"` are implemented today — setting it to anything else
fails the configure step with a clear message rather than silently building nothing. `NullFrameBackend` itself is
header-only and always available regardless of this option; the option instead gates which *real* backend (issue
#69, `Sdl3FrameBackend` issue #151 being the first) is compiled in alongside it. Most CI runners have no real GPU
or display hardware, so the default keeps the mechanism up to the backend boundary fully buildable/testable
without ever touching SDL3, with the real GPU dependency only fetched/linked on a build that explicitly opts in
(`-DATLAS_RENDER_BACKEND=SDL3`) — see `libraries/atlas-render/CMakeLists.txt`.

**SDL3 is fetched via `FetchContent`, `SYSTEM`, statically linked, only when `ATLAS_RENDER_BACKEND=SDL3`.**
Follows this project's existing yaml-cpp/GoogleTest `FetchContent` precedent (`tools/atlas-cgen/CMakeLists.txt`):
`SYSTEM` so SDL3's own header warnings never fail this project's `-Werror` gate (CLAUDE.md's documented reason —
`.clang-tidy`'s `HeaderFilterRegex` only suppresses clang-tidy's own diagnostics for third-party headers, not the
compiler's), `SDL_SHARED=OFF`/`SDL_STATIC=ON` so no shared library needs to travel alongside test/tool binaries at
run time, `SDL_TEST_LIBRARY=OFF` since this project has no use for SDL's own test harness. Its fetched targets are
explicitly exempted from `CMAKE_CXX_CLANG_TIDY`, the same as yaml-cpp/GoogleTest, so the clang-tidy gate never
reaches into a dependency's own source. Pinned to `release-3.4.12` (verified to exist against the real
`libsdl-org/SDL` tag list, not assumed). **Building `ATLAS_RENDER_BACKEND=SDL3` on Linux needs the OS-level X11
development headers SDL3's own CMake build lists as dependencies** (`libx11-dev`, `libxext-dev`, `libxcursor-dev`,
`libxi-dev`, `libxfixes-dev`, `libxrandr-dev`, `libxss-dev`, `libxtst-dev`, roughly — see
<https://wiki.libsdl.org/SDL3/README-linux#build-dependencies> for the authoritative list) beyond what this
repository's default `NULL` build has ever required; since the default build is and remains completely
unaffected, wiring an extra package-install step into CI's job matrix for the `SDL3` configuration is left as
follow-up rather than done speculatively here.

**Issue #153's shader toolchain went through two rounds of scoping correction, both driven by actually trying
the plan against real upstream sources rather than trusting a summary of them — see
`libraries/atlas-render/cmake/FetchShaderTooling.cmake` for the implementation this settled on.**

- *Round 1 ("SPIR-V only, `SDLSHADERCROSS_DXC=OFF`") was infeasible*, discovered by reading `SDL_shadercross`'s
  real `src/SDL_shadercross.c` at the pinned commit: DXC is not an optional accelerator for one output format,
  it is `SDL_shadercross`'s **only HLSL front-end** — `SDL_ShaderCross_CompileSPIRVFromHLSL()` unconditionally
  routes through it. Disabling DXC makes every HLSL compile call fail deterministically, on any machine, GPU or
  not — SPIRV-Cross (the apt-installable piece) only transpiles *from* already-compiled SPIR-V, it has no HLSL
  parser at all.
- *Round 2's plan ("prebuilt DXC binaries, apt package for SPIRV-Cross") was half right.* The prebuilt-DXC-binary
  mechanism — fetching Microsoft's official `v1.9.2602` release the exact way `SDL_shadercross`'s own
  `build-scripts/download-prebuilt-DirectXShaderCompiler.cmake` does (same URL, same SHA-256, independently
  re-verified with a real download in this round, not copied blind) — worked exactly as planned: this project's
  own `FetchContent_Declare(... URL ... URL_HASH ...)` reproduces it directly, no vendoring of DXC from source.
  **But the apt package for SPIRV-Cross (`libspirv-cross-c-shared-dev`, Ubuntu noble universe) turned out to be
  too old** — verified empirically by actually trying it, not assumed from "confirmed installable" alone: linking
  `SDL_shadercross` against it failed with roughly 90 real compiler errors (`spvc_msl_resource_binding_2`,
  `SPVC_COMPILER_OPTION_HLSL_USE_ENTRY_POINT_NAME`, and other symbols this pinned `SDL_shadercross` commit calls
  simply don't exist in that 2021-vintage package). Re-reading `SDL_shadercross`'s own CI
  (`.github/workflows/main.yml`) confirmed its Ubuntu 24.04 leg doesn't rely on an apt package either — it builds
  SPIRV-Cross from source as a separate step, entirely outside `SDL_shadercross`'s own `SDLSHADERCROSS_VENDORED`
  machinery (which would build SPIRV-Cross *and* DXC together from git submodules inside `SDL_shadercross`'s own
  subdirectory — a materially different, heavier path this project does not take). This project's own
  `FetchShaderTooling.cmake` does the same: fetches SPIRV-Cross at the exact commit `SDL_shadercross`'s own
  `.gitmodules` submodule pointer resolves to at the pinned commit (`git ls-tree <commit> external/SPIRV-Cross`,
  not the floating `main` branch `.gitmodules` itself names — `1a6169566c73d3da552748fc372fe2bbb856e46e` today),
  and configures/builds/installs it as a standalone `execute_process` sub-build (a real `cmake --install`,
  guarded by a stamp file so an ordinary reconfigure doesn't recompile it every time) — `find_package(spirv_cross_c_shared)`
  needs a genuine installed CMake package, which `add_subdirectory()`-ing SPIRV-Cross into this project's own
  build graph would never produce.
- **DXC and SPIRV-Cross are both discovered via environment variables, not `-D` cache flags — verified
  empirically, not assumed from a first read of `SDL_shadercross`'s own `CMakeLists.txt`.** That file
  unconditionally does `set(DirectXShaderCompiler_ROOT ...)` (a plain, non-`CACHE` variable) immediately before
  its own `find_package(DirectXShaderCompiler REQUIRED)` call — which would shadow any `-D` cache value passed on
  the command line, in that same directory scope. CMake's `<PackageName>_ROOT` policy (`CMP0074`) makes
  `find_package()`'s own nested `find_path`/`find_library`/Config-mode search also consult
  `$ENV{<PackageName>_ROOT}` regardless of that shadowing — which turns out to be exactly how `SDL_shadercross`'s
  own CI sets these two variables too (`GITHUB_ENV`, i.e. the *process environment* of the subsequent configure
  step), not a command-line flag. `FetchShaderTooling.cmake` does the same via `set(ENV{...} ...)` before
  `FetchContent_MakeAvailable(SDL_shadercross)`.
- **HLSL is compiled at `Sdl3FrameBackend` construction time (runtime), not by a separate offline build-time
  tool.** This round has no offline shader-compiler tool of its own (`SDLSHADERCROSS_CLI` is left `OFF` — building
  one would be a distinct, arguably separate `atlas-` tool), and `SDL_ShaderCross_CompileSPIRVFromHLSL()` is fast
  enough for a shader this small (single-digit milliseconds) that doing it once per construction is not a
  meaningful cost against this round's "prove the mechanism, not production-ready" bar. A real production render
  loop would likely want this precompiled at build time instead — left as a real, later design question (see
  Open Questions) once `atlas-rcc`-style offline tooling exists for shaders the same way it does for resources.
- **`FetchContent`-fetched SDL3's guard against `SDL_shadercross`'s own `find_package(SDL3)` call works as
  expected — verified empirically, not assumed.** `SDL_shadercross`'s `CMakeLists.txt` guards that call behind
  `if(NOT (TARGET SDL3::Headers AND TARGET SDL3::SDL3 AND (TARGET SDL3::SDL3-static OR TARGET SDL3::SDL3-shared)))`;
  since this library's own SDL3 fetch (above) already defines all three of those target names before
  `FetchShaderTooling.cmake` runs, `SDL_shadercross`'s own configure output shows no `"Found SDL3"` message at
  all in this round's real build — confirmed by grepping the actual configure log, not inferred from reading the
  guard condition alone.
- Only the Linux/SPIRV-Cross/Vulkan path is exercised end-to-end by this round's own sandbox/CI, same caveat
  shape as #151's own cross-platform note — see Open Questions for why macOS/Windows remain unverified here.

**HLSL sources live under `libraries/atlas-render/shaders/`, and the two new build-time CMake scripts under
`libraries/atlas-render/cmake/`** — both new directories for this library, matching this project's own
`tools/atlas-cgen/templates/`+`tools/atlas-cgen/cmake/scripts/` precedent for "a source asset plus the script
that turns it into a compiled-in C++ artifact" rather than mixing either into `include/`/`src/`. `cmake/EmbedShaderSource.cmake`
reimplements `tools/atlas-cgen/cmake/scripts/EmbedTextFile.cmake`'s exact technique (a `constexpr std::string_view`
generated header) rather than sharing that script directly, parameterized by C++ namespace, so this library's own
shader-embedding stays independent of `atlas-cgen`'s tooling — no cross-tool coupling for what is otherwise a
completely generic "embed a text file" operation. `cmake/FetchShaderTooling.cmake` is a separate file from this
library's own `CMakeLists.txt` (rather than growing that file substantially in place) specifically because it is
the one part of this round's CMake logic with real, non-trivial control flow (a conditional platform guard, an
`execute_process` sub-build with a stamp-file guard) — CLAUDE.md's own "small, single-purpose modules" principle
applied to build logic, not just C++.

**Before issue #155, this round drew its own boundary at "the draw call executes without error" — it did not
verify what actually lands on screen; #155 closes that gap.** `tests/atlas-render/sdl3_frame_backend_test.cpp`'s
`Sdl3FrameBackendTest.SubmitDrawsARealResolvedDrawCommandWithoutThrowing` submits a real `Frame`/`DrawCommand`
against a real `ResourceRegistry` (built from real packed fixture blobs wrapping the existing
`triangle.mesh`/`checker.tex` fixtures) into the real window's swapchain and asserts only that it doesn't throw
and eventually reports completion — proving the full resolve → decode → GPU-upload → draw mechanism executes
cleanly end-to-end, but never that the rendered pixels are a correctly textured triangle. Issue #155 is the
"spinning box" visual test and the genuine pixel-correctness proof this gap called for — see the two paragraphs
below for the verified mechanism and `tests/atlas-render/sdl3_pixel_correctness_test.cpp`/
`sdl3_spinning_box_test.cpp` for what it actually built.

**`Sdl3FrameBackend`'s own tests decide "no real GPU/display" via genuine construction failure, not a mocked
driver — the "Headless CI" open question this issue asked to be resolved, not silently skipped.** Concretely:
every test attempts *real* `SDL3`/`SDL_GPU` window and device creation; `SDL_HINT_VIDEO_DRIVER` is set to
`"offscreen"` (SDL3's own offscreen video backend, `src/video/offscreen/` — **not** `"dummy"`, issue #151/#153/#154's
original choice; see the next paragraph for why that was incomplete) first so windowing itself always succeeds
headlessly (no `DISPLAY`, no real compositor needed). Each test (a `Sdl3FrameBackendTest` GTest fixture,
`tests/atlas-render/sdl3_frame_backend_test.cpp`) attempts real construction in `SetUp()` and calls
`GTEST_SKIP()` — logging the thrown exception's message, not swallowing it — the moment that fails, rather than
either mocking `SDL_GPU` out (which would stop testing the real API surface entirely) or leaving the test suite
red on every machine without a GPU. On a machine with a working `SDL_GPU` backend (this project's own dev
sandbox included, as of issue #155 — see below), the exact same tests exercise the real success path
(window/device creation, clear-and-present, real fence completion, real shader compilation and GPU draw calls)
instead of skipping. A separate, always-runnable test
(`Sdl3FrameBackendConstruction.FailureReportsSdlErrorTextInTheException`) forces a *deterministic* failure — an
invalid `SDL_HINT_VIDEO_DRIVER` value, not dependent on the sandbox's own GPU/Vulkan-ICD state — so the
"construction failure surfaces a real error message" behavior itself has coverage on every machine, GPU or not.

**Issue #155: `"dummy"` was never going to work here, regardless of GPU/Vulkan-ICD availability — a distinct,
more complete finding than "no GPU in this sandbox," which was real but incomplete.** Reading SDL3's own source
directly (`src/gpu/vulkan/SDL_vulkan.c`, `VULKAN_PrepareDriver`) shows the `"dummy"` video driver has **no
`Vulkan_CreateSurface` implementation at all** — `SDL_GPU`'s Vulkan backend unconditionally checks for that and
bails out before even attempting device creation. Every GPU-dependent test across #151/#153/#154
(`sdl3_frame_backend_test.cpp`, `mesh_upload_cache_test.cpp`, `texture_upload_cache_test.cpp`) used `"dummy"`
and always skipped in this sandbox — true, but for a more structural reason than "no Vulkan ICD is installed": a
software Vulkan implementation alone was never going to fix that skip path. The verified fix (issue #155, three
parts, each confirmed for real rather than assumed):
1. **`mesa-vulkan-drivers`** (a real Debian/Ubuntu apt package, already present in this sandbox's shared
   container) provides Mesa's `lavapipe`/`llvmpipe` software Vulkan ICD, auto-registered under the standard
   `/usr/share/vulkan/icd.d/` path.
2. **`SDL_HINT_VIDEO_DRIVER` = `"offscreen"`** (SDL3's own offscreen video backend, `src/video/offscreen/`)
   instead of `"dummy"` — `"offscreen"` *does* implement `Vulkan_CreateSurface`. With both (1) and (2),
   `SDL_CreateGPUDevice` genuinely succeeds against `llvmpipe` — confirmed via a standalone probe against the
   real fetched SDL3 build (`SDL_GPU Driver: vulkan`, and per the next paragraph, a real render+readback against
   it).
3. **Caveat, also verified**: `"offscreen"`'s window has no real presentable surface, so
   `SDL_WaitAndAcquireGPUSwapchainTexture` still legitimately returns a null texture (the same "no presentable
   image this frame" case a minimized real window produces) — swapchain-based pixel verification does not work
   under `"offscreen"`. The fix, used by `sdl3_pixel_correctness_test.cpp`: render into an explicit off-window
   `SDL_GPUTexture` (this library's own established pattern, e.g. the now-deleted `sdl3_shader_pipeline_test.cpp`'s
   `DrawInsideARealRenderPassDoesNotThrowOrCrash`, git history at `93107fb`) and read it back via
   `SDL_DownloadFromGPUTexture` + an `SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD` transfer buffer + a copy pass +
   `SDL_SubmitGPUCommandBufferAndAcquireFence` + `SDL_WaitForGPUFences` + `SDL_MapGPUTransferBuffer`. Verified
   directly (a standalone probe, before writing any test) by clearing a small off-window texture to opaque red
   and downloading the exact bytes `R=255 G=0 B=0 A=255` back.

With all three confirmed, `sdl3_pixel_correctness_test.cpp` draws a textured quad through the real
`Sdl3MeshPipeline`/`MeshUploadCache`/`TextureUploadCache` production code (not a reimplementation of any part of
it — see that test's own doc comment for why it calls those directly rather than through
`Sdl3FrameBackend::submit()`, which only ever targets the window's own swapchain) and asserts the four interior,
edge-artifact-free sample pixels match `checker.tex`'s four quadrants exactly — the first genuine proof in this
library that a `FrameBackend` renders *correct* pixels, not merely "executes without throwing."
`sdl3_spinning_box_test.cpp` is the companion "every `FrameBackend` ships a smoke test" convention deliverable:
one entity with a continuously-rotating `Transform`, driven through the real `build_frame` →
`Sdl3FrameBackend::submit()` loop for many ticks, asserting mechanism stability (no throw, no crash, real GPU
work genuinely completing) rather than any particular pixel — predating issue #181's real `Camera`, and still
deliberately not updated to add one: this test's own point is mechanism stability across many ticks, not visual
correctness (pixel correctness is `sdl3_pixel_correctness_test.cpp`'s job, with camera-driven proof now covered by
its own dedicated tests, see "What's implemented" above), so composing a `Camera` in here as well would only
duplicate that coverage without strengthening either test.

**Two further, distinct third-party leaks surfaced by making these tests genuinely execute for the first time —
both verified and suppressed, neither this project's own code.** Real shader compilation (`SDL_shadercross`'s
HLSL→SPIR-V path) and real device creation against lavapipe were, by construction, unreachable code as long as
every fixture-based test always skipped under `"dummy"` — issue #155's retrofit is what first exercised them for
real, and LeakSanitizer (on by default in the sanitized `debug` preset) caught two genuine leaks neither
attributable to this library: one inside the vendored, prebuilt DirectXShaderCompiler shared library
(`libdxcompiler.so`, fetched by `SDL_shadercross`'s own CMake) on every real HLSL→SPIR-V compile, and one on a
background thread spawned during real lavapipe/LLVM device/JIT initialization. Both are suppressed via
`tests/atlas-render/lsan_suppressions.txt` (wired in only for the `SDL3` backend's test binary,
`tests/atlas-render/CMakeLists.txt`) rather than silently disabling leak detection wholesale — see that file's
own doc comment for the full writeup of each, including why the second could only be suppressed by a broader
pattern than the first (the offending shared object is already `dlclose()`'d, and so unresolvable to a specific
function, by the time `LeakSanitizer`'s exit-time check runs). `tests/atlas-render/CMakeLists.txt` also pins
`VK_ICD_FILENAMES` to lavapipe's own ICD JSON directly (guarded by `EXISTS`, falling back to the loader's default
enumeration if absent) — this sandbox has several other registered-but-nonfunctional ICDs
(`gfxstream`/`nouveau`/...) for hardware that is not actually present, and letting the Vulkan loader probe them
was directly observed to be the source of that second leak, not lavapipe itself.

**No real GPU/windowing backend before issue #151 — deliberately deferred, per issue #30's explicit scope.**
Before #151, no new third-party dependency (no SDL/Vulkan/bgfx/sokol) had been introduced. `build_frame` proves
the actual mechanism — consuming real composed property/resource state and producing a deterministic, testable
frame descriptor — without needing a display, a window, or a GPU context. Wiring `Frame`'s `DrawCommand` list
all the way into real GPU draw calls (issue #154, this round) needed a real asset resolver
(`atlas::resource::ResourceRegistry`, already built by issue #66) and the mesh/texture decode step (issue #152)
first — everything in this round is real machinery up to and including that boundary, not a stand-in for it.

**A null `ResourceId`, and now a genuine resolution/decode/upload failure, both mean "skip this DrawCommand" —
never substitute a placeholder draw.** `build_frame` already treats a null `Renderable::mesh`/`::material` as
unresolved and skips it before a `DrawCommand` is even produced (frame_builder.hpp's own documented convention).
Issue #154 extends the same "skip, never substitute" stance one layer further: `Sdl3FrameBackend::submit()`
additionally skips any surviving `DrawCommand` whose mesh or texture resolves to a non-null id but still fails
somewhere in `MeshUploadCache`/`TextureUploadCache`'s own resolve → decode → GPU-upload chain (unresolved,
resolution-failed, malformed/truncated bytes, or a genuine GPU upload failure) — a *resolvable-but-failed*
reference is no longer the purely hypothetical case the original text here flagged as a future increment; it is
now a real, tested code path (`Sdl3FrameBackendTest.SubmitSkipsADrawCommandWithAnUnresolvedMeshWithoutThrowing`/
`...UnresolvedTextureWithoutThrowing`, `MeshUploadCacheTest`/`TextureUploadCacheTest`'s own failure-status
tests).

**Rotation interpolation is `nlerp`, never `slerp` — a direct consequence of spec §4, not a stylistic choice.**
`slerp` needs `acos`/`sin`, and unlike the four arithmetic operations and `std::sqrt` (which IEEE-754 requires
to be correctly rounded), the standard does not require transcendental functions to be correctly rounded — two
conforming platforms' `libm` (glibc on Debian 13, macOS's on Apple Silicon, the Windows CRT on x86-64 — exactly
CLAUDE.md's three deployment targets) can legitimately disagree in the last bit or more. `slerp` would have
silently reintroduced cross-platform floating-point divergence in presentation code the moment two machines
interpolated the same rotation. `nlerp` (linear interpolation of the four components, then re-normalize via
`std::sqrt`) avoids every non-required-to-be-correctly-rounded function, at the cost of non-constant angular
velocity along the interpolated path — an acceptable trade for presentation-only rendering, never one this
library would accept for anything feeding back into simulation state.

**`nlerp` falls back to the second rotation rather than dividing by (near) zero.** Two rotations whose
component-wise sum has near-zero length only arises from exactly opposing quaternions (`b == -a`
component-wise) — both represent the same rotation under quaternion double-cover, so returning `b` unmodified
is a safe, deterministic fallback, not a meaningfully different rotation being silently substituted. This is a
real, tested branch (`Nlerp.OppositeQuaternionsFallBackToTheSecondRotationRatherThanDividingByZero`), not
speculative defensiveness.

**`build_frame` takes `std::span<const EntityRef>`, never discovers "which entities exist" itself.** Deciding
what counts as a renderable entity in a given scene is a capability/host-composition concern (§5) this library
has no business owning (spec §2, Mechanism Over Meaning) — `atlas-render` only resolves state for whatever
entities it's handed, in the exact order it's handed them. That ordering guarantee is what makes `build_frame`
itself trivially deterministic (spec §4): a `std::span`'s iteration is inherently sequential, never derived
from an `unordered_map`'s own iteration order, so identical inputs always produce a bit-identical `Frame` —
tested directly (`BuildFrame.MultipleEntitiesAreVisitedInTheCallerSuppliedOrder`,
`BuildFrame.RepeatedCallsWithIdenticalInputProduceIdenticalOutput`).

**`Frame`/`DrawCommand`/`Renderable`/`Transform` deliberately have no comparison operators.** None of them are
used as map/set keys, and every test that needs to compare produced state does so field-by-field (matching this
repo's own precedent, e.g. `atlas-runtime`'s `property_composition_test.cpp`) rather than via a defaulted
`operator==`/`<=>` this library would otherwise need to maintain without an actual caller needing it.

**The mesh/texture formats (issue #152) are hand-rolled, mirroring `atlas-audio`'s WAV decision (issue #55) —
no third-party asset importer was introduced.** `ResourceRegistry::resolve()` (`atlas-resource`) hands back raw
bytes; nothing decoded them into GPU-upload-ready data before this round. Assimp/tinyobjloader (mesh) and a
real image codec (PNG/DDS, texture) were deliberately not reached for — both chosen formats are a fixed header
plus one or two flat arrays, exactly the "trivial enough that a dependency wouldn't earn its keep" bar
`atlas-audio`'s README sets for WAV. The exact layouts, documented in full on each decode function:

```
mesh:     u32 vertex_count
          u32 index_count
          vertex_count x { float px,py,pz; float nx,ny,nz; float u,v; }  -- 32 bytes each
          index_count  x { u32 index }

texture:  u32 width
          u32 height
          width * height x { u8 r, g, b, a }   -- row-major, top row first, no padding
```

Integers/floats are read host-native, the same explicit assumption `atlas::rcc::pack_resource_blob` already
states (little-endian on every deployment target this project ships to — none is big-endian in practice).

**`decode_mesh`/`decode_texture` return `std::optional`, never throw, mirroring `ResourceRegistry::resolve()`'s
own three-way status.** A corrupted or truncated asset is an ordinary runtime condition a host observes at
load time — distinct from the parse-time exceptions `atlas-cgen`/`atlas-rcc` throw for malformed *build-time*
input (manifests, blob-packing input), per CLAUDE.md's `std::expected`-avoidance note. Both functions validate
that every byte their declared header counts/dimensions imply actually exists in the input span before reading
any of it, rather than reading first and hoping — `tests/atlas-render/mesh_asset_test.cpp` and
`texture_asset_test.cpp` cover a well-formed decode, a truncated header, and truncated payload data for each,
using real fixture files on disk under `tests/atlas-render/fixtures/` (matching
`tests/atlas-resource/resource_registry_test.cpp`'s own fixture-file convention) rather than mocked byte
arrays.

**`decode_texture`'s size check is overflow-safe by construction, not by luck.** `width` and `height` are each
`u32`, so their product always fits `std::size_t` (64-bit on every deployment target this project ships to)
without overflowing — but that product multiplied by 4 bytes-per-pixel can still overflow for adversarial huge
dimensions (e.g. `width == height == 0xFFFFFFFF`), and a naive `width * height * 4` would silently wrap around
into a small, incorrectly "valid"-looking byte count, passing the truncation check and then reading out of
bounds. `decode_texture` guards this explicitly with a division-based comparison before ever computing the
byte count, tested directly
(`DecodeTexture.AdversarialDimensionsThatWouldOverflowTheSizeComputationFailToDecode`). `decode_mesh` has no
equivalent guard: its two format fields (`vertex_count`, `index_count`) each only ever multiply against a
small fixed per-element size, never against each other, so the same overflow can't arise there.

**`Vertex` reuses `atlas::core::Vec3` for `position`/`normal` rather than a duplicate 3-float type.** Both
are already plain, GPU-conventional-precision `float` triples with no invariant of their own — introducing a
second identical-shaped struct just for mesh data would be duplication without a distinguishing reason.

**Issue #228's skeleton format, and the acyclic-by-construction hierarchy invariant its decoder enforces:**

```
skeleton: u32 joint_count
          joint_count x {
              u32 parent_index
              float px, py, pz          -- bind-pose position
              float qx, qy, qz, qw      -- bind-pose rotation (quaternion)
              float sx, sy, sz          -- bind-pose scale
          }                                                    -- 44 bytes each
```

Hand-rolled for the same reason `decode_mesh`/`decode_texture` are — a fixed header plus one flat array of
fixed-size records is trivial enough that a third-party skeletal-animation importer would not earn its keep for
this format alone. `Joint.bind_pose` reuses `atlas::render::Transform` (`transform.hpp`) verbatim rather than a
parallel skeleton-specific transform type, the same "don't duplicate an identical-shaped type" stance
`Vertex.position`/`.normal` already take for `atlas::core::Vec3`.

**The required hierarchy invariant — every joint's `parent_index` is either `no_parent_joint` or strictly less
than that joint's own index — is the entire validation this format needs, deliberately chosen so that a general
graph cycle-detection algorithm is never necessary.** Requiring parents to precede their children in authoring
order makes the hierarchy acyclic by construction: a chain of parent pointers from any joint can only ever walk
toward strictly decreasing indices, so it must terminate at `no_parent_joint` within at most `joint_count` steps
— an infinite or out-of-bounds walk is structurally impossible for any input `decode_skeleton` accepts, not
merely unlikely. This also naturally forces joint 0 to always be a root (there is no earlier index it could
legally reference), so "the hierarchy has at least one root" falls out of the same single check
(`parent_index == no_parent_joint || parent_index < joint_index`) rather than needing to be verified separately.
`decode_skeleton` checks this per joint as it decodes, before that joint is ever committed to the output
vector — rejecting (`std::nullopt`) at the first violation, mirroring `decode_mesh`/`decode_texture`'s own
"validate before trusting" stance for truncated input.

**`no_parent_joint` is `std::numeric_limits<std::uint32_t>::max()`, a named constant rather than a magic number
repeated at call sites.** A dedicated `bool is_root` field per joint was considered and rejected: it would let a
malformed blob set `is_root = true` and a nonsensical, non-`no_parent_joint` `parent_index` simultaneously — a
state the hierarchy invariant above could not catch. A single sentinel index keeps "is this joint a root" and
"who is its parent" the same field, the same way `atlas-resource`'s null `ResourceId` already represents
"absent" as a sentinel value of the same type rather than a parallel validity flag.

**No overflow guard on `joint_count * 44`, mirroring `decode_mesh`'s own stance, not `decode_texture`'s.**
`joint_count` is `u32` and only ever multiplies against a small fixed per-record size (44 bytes) — it never
multiplies against another independently-adversarial declared count the way `decode_texture`'s
`width * height` does, so the same reasoning `decode_mesh.cpp`'s own doc comment gives for skipping an overflow
guard on `vertex_count`/`index_count` applies here unchanged.

## Open questions (flagging for human review, not silently resolved)

- **Camera *collision* remains unimplemented — the direct next sub-issue, #182, now that both this issue (#181)
  and #180's raycast/sweep query exist.** A real `Camera`/view-projection concept now exists (issue #181, this
  round), and #180 already built the sweep query it would need — but wiring the two together (e.g. sweeping the
  camera's own position against `atlas-physics` obstacles to avoid clipping through geometry) is explicitly out
  of this round's scope (see this issue's own "Explicitly out of scope") and left entirely to #182.
- **True frustum/camera-aware culling remains unimplemented — the direct follow-up now that a real `Camera`
  exists (issue #181 closes the gap issue #156's own distance-culling design deliberately worked around, see
  "Scoping decisions" above).** A straightforward extension of the same compute → indirect-draw mechanism #156
  built, just testing each `DrawCommand`'s bounds against the active camera's own view-projection frustum planes
  (`to_view_projection_matrix`, `camera.hpp`) instead of a fixed distance from a reference point — genuine,
  unaddressed follow-up work, not attempted this round.
- **Multiple simultaneous cameras/viewports/split-screen remain unimplemented — issue #181's own explicit scope
  boundary, the same "your call, document your choice" latitude issue #156 used for its own scoping calls.**
  `Sdl3FrameBackend` owns exactly one active `Camera`, replaced wholesale by `set_camera()`; nothing renders more
  than one color target or composes more than one view-projection matrix per `submit()` call. A future round
  wanting split-screen or multiple render targets would need a materially different `Sdl3FrameBackend` design
  (per-viewport camera + render-target pairs, at minimum) — left entirely undesigned here.
- **No batching of `DrawCommand`s that share the same mesh into a single `draw_count=N` indirect draw call —
  issue #156's own explicit scope boundary, not an oversight.** Every resolved `DrawCommand` gets its own
  `draw_count=1` indirect draw call (see "Scoping decisions" above for why), even when many consecutive
  `DrawCommand`s reference the identical mesh (and so could, in principle, share one bound vertex/index buffer
  and one indirect multi-draw call spanning all of them, with the compute pass writing each one's own
  `num_instances` into that shared call's own entry array). Grouping `DrawCommand`s by mesh `ResourceId` and
  issuing one indirect call per group would reduce the number of draw calls per frame without needing the
  heavier "merge every mesh into one shared buffer" redesign — a real, unaddressed follow-up, the same "no
  batching policy" stance this library already takes for individual per-draw pipeline/state sorting (below).
- **The distance-cull pass's GPU buffers (input storage buffer, its transfer buffer, the indirect buffer) are
  allocated fresh and torn down every single `submit()` call rather than pooled/reused across frames — a real,
  documented perf trade-off (see "Scoping decisions" above), not an oversight.** A production render loop
  submitting many frames per second would likely want a persistent, capacity-tracked buffer pool instead (grown,
  never shrunk, as the largest-seen frame's `DrawCommand` count increases) to avoid the repeated
  `SDL_CreateGPUBuffer`/`SDL_CreateGPUTransferBuffer` allocation churn this round accepts — doing so safely
  needs care around not releasing/regrowing a buffer a prior frame's still-in-flight GPU work might reference
  (this round's own per-frame fresh-allocate-then-release-after-submit avoids that hazard entirely by
  construction, at the cost of the allocation churn itself), left as real follow-up for whoever needs the
  throughput.
- **No cache eviction policy exists for `MeshUploadCache`/`TextureUploadCache` — flagged as unresolved, the
  same honesty issue #133's/#164's equivalent open questions already established for the audio/resource side.**
  Both caches grow unboundedly for the lifetime of the `Sdl3FrameBackend` that owns them: a mesh or texture
  referenced once and never again keeps its GPU buffer/texture alive (and its cache entry resident) until
  `release()` tears the whole cache down at backend destruction. A real long-running host with a large,
  changing working set of assets (e.g. streaming levels) will eventually need an eviction policy (LRU, explicit
  unload, reference counting) neither cache attempts here.
- **Every draw this round rebinds the same `Sdl3MeshPipeline` and re-pushes a fresh model-matrix uniform per
  `DrawCommand`, with no batching, instancing, or pipeline-state sorting.** Functionally correct (SDL_GPU
  permits redundant `SDL_BindGPUGraphicsPipeline` calls), but not the shape a production renderer would want
  once the number of draws per frame grows large — sorting draws by pipeline/material to minimize state changes,
  or instancing repeated meshes, is real, unaddressed follow-up work, the same "no batching policy" stance this
  library already takes for its own `FrameBackend::last_completed_tick()` design (see below).
- **Issue #153's shader toolchain is only verified on Linux/SPIRV-Cross/Vulkan — macOS/Windows remain unbuilt
  and unverified here, for a different reason on each platform.** On Windows, Microsoft does publish a prebuilt
  DXC binary (`dxc_2026_02_20.zip`, the same release), so `FetchShaderTooling.cmake` would need a Windows branch
  mirroring the Linux one (different path suffixes per `FindDirectXShaderCompiler.cmake` — `bin`/`lib` under
  `windows/` rather than `linux/`) — not attempted this round, and the CMake module currently fails its
  configure step outright on any non-Linux `CMAKE_SYSTEM_NAME` rather than silently doing something untested. On
  macOS, Microsoft publishes no prebuilt DXC binary at all — `SDL_shadercross`'s own CI builds DXC from source
  there instead (`vendored: true` on its macOS leg), a materially heavier, unattempted path here. Whoever picks
  this up next needs to decide whether macOS support is worth vendoring DXC from source for, or stays a Linux
  (and eventually Windows) -only capability.
- **CI needs three things this round to actually exercise `ATLAS_RENDER_BACKEND=SDL3` end-to-end, none of them
  wired here per this round's own constraints (deliberately left to the coordinating session/CI change):**
  the `libspirv-cross-c-shared-dev` apt package is *not* actually needed by the build itself (SPIRV-Cross is
  built from source, see "Scoping decisions" above) — the apt package was this round's original, since-corrected
  assumption; the real prerequisite is a plain C++ toolchain able to configure/build SPIRV-Cross (already true of
  every runner building this project at all), and network access to `github.com` (for `SDL_shadercross`,
  SPIRV-Cross, and the DXC binary download) and `release-assets.githubusercontent.com`/`objects.githubusercontent.com`
  (redirect target for the DXC binary's `URL`) during configure. The DXC prebuilt-binary download adds one
  ~13&nbsp;MB fetch; building SPIRV-Cross from source adds roughly 30 seconds to a fresh configure (this round's
  own sandbox measurement) but is skipped on every subsequent reconfigure of an existing build tree (a stamp
  file guards it, see `FetchShaderTooling.cmake`) — a real CI cache of `build/*/libraries/atlas-render/spirv-cross-install`
  would avoid paying it even on a fresh checkout.
- **HLSL is compiled at runtime (`Sdl3FrameBackend` construction), not offline at build time — see "Scoping
  decisions" above for why this round chose that.** A future round wanting build-time shader compilation would
  need either `SDLSHADERCROSS_CLI=ON` plus a `shadercross` CLI invocation wired into this library's own
  `CMakeLists.txt` (an `add_custom_command` emitting precompiled `.spv`/`.dxil` alongside the embedded HLSL
  source, mirroring `atlas-cgen`'s own generated-artifact pattern), or a small dedicated `atlas-` shader-compiler
  tool of its own (per CLAUDE.md's `tools/` naming convention) — left open for whoever needs it.
- **No skeletal animation, texture compression/mipmaps, or richer material data — issue #152's explicit
  scope.** `Vertex` is position/normal/UV only (no bone indices/weights, tracked separately per #45/#112);
  `DecodedTexture` is a single uncompressed RGBA8 layer (no mip chain, no PBR parameter set, no multiple
  texture slots). A first round needs just enough to draw one textured mesh — richer data is a real design
  question for whoever picks this back up, not silently precluded by anything in this format.
- **Issue #228's `SkeletonAsset` has no vertex-to-joint binding (skinning weights), no GPU skinning, and no
  retargeting between mismatched skeletons — all explicitly out of this issue's scope.** `Vertex` (above)
  still carries no bone indices/weights of its own; a future mesh-format extension adding them (and the actual
  GPU skinning pass consuming both a `SkeletonAsset` and a per-vertex bone binding) is real, unaddressed
  follow-up work, not attempted here. Nothing yet references a `SkeletonAsset` from an actual `Animation`
  resource either — this issue's own stated purpose is only to give that upcoming sibling `atlas-rcc` issue a
  `skeleton:`-resolved schema to index a per-joint pose set against, not to build the animation sampling/
  playback machinery itself.
- **No production import pipeline (Assimp or similar) — deliberately out of scope.** This is a from-scratch
  minimal format for this project's own authoring/build pipeline, not a general importer for arbitrary
  externally-authored assets. Getting real content into this format (e.g. a Blender export step, or a small
  offline converter from glTF) is unaddressed here and left to whoever builds the authoring-side tooling.
- **GPU upload now exists (issue #154, `MeshUploadCache`/`TextureUploadCache`) but has only been exercised via
  real construction attempts that fail cleanly in this sandbox (no `/dev/dri`, no Vulkan ICD) — never against a
  successful upload.** Every GPU-upload code path (buffer/texture creation, transfer-buffer map/copy/submit) is
  written against the real `SDL_GPU` API and compiles/links against it, but this round's own CI/sandbox has
  never actually run it to completion; a machine with a working `SDL_GPU` backend (or a future Lavapipe/
  SwiftShader-equipped CI runner) would be the first environment to exercise the real success path end-to-end.
- `build_frame`'s own "null id means unresolved" rule (§13, `atlas-resource`'s own scoping note) still concerns
  itself only with *build_frame's* boundary — deciding whether a `Renderable` counts as complete enough to
  produce a `DrawCommand` at all. Issue #154 answers the next layer down (what a backend does with a
  *non-null-but-still-failing* reference: skip it, per `MeshUploadCache`/`TextureUploadCache`'s own status
  enums), so this is no longer an open design question for `Sdl3FrameBackend` specifically — it remains one for
  any *other* future backend that might want a different policy (e.g. substituting a debug placeholder mesh
  instead of skipping, for a development-only build configuration).
- No actual double-buffered "previous tick" state exists in this round — `lerp`/`nlerp` are pure functions a
  caller can already use today (given two `Transform`s from whatever ticks it's tracking itself), but this
  library doesn't yet own storing "the previous tick's resolved state" itself. Whether that becomes a
  responsibility of a future `atlas-render` frame-history type, or stays the calling host's own concern, is
  left open.
- **No batching/frame-skip policy exists, by design.** `FrameBackend::last_completed_tick()` (issue #148) gives
  a caller the raw signal needed to decide whether to build/submit another `Frame` yet, but `atlas-render`
  deliberately has no opinion on the policy built on top of it (how many ticks behind is "too many," skip vs.
  interpolate) — left entirely to whoever composes the render loop, the same division of responsibility issue
  #117 drew for culling.
- Cross-platform target support (Debian 13 primary, macOS ARM, Windows x86-64, per issue #30's acceptance
  criteria) is confirmed at the *language/standard-library* level this round: everything here is plain C++23
  (`<cmath>`, `<span>`, `<vector>`) with no platform-conditional code, and the `nlerp`-over-`slerp` decision
  above is specifically the floating-point precaution needed for that claim to hold under this project's own
  three-libm reality. It has not been verified by an actual CI build on macOS/Windows in this round — this
  repository's existing CI matrix (`.github/workflows/ci.yml`) already builds/tests release configurations on
  macOS and Windows for every library uniformly, so `atlas-render` inherits that coverage the same way every
  other library here does, rather than needing a bespoke check of its own.
- **`Sdl3FrameBackend`'s cross-platform GPU backend choice (Vulkan/Metal/D3D12) has only been exercised on
  Linux/Vulkan so far** (this sandboxed dev environment's own `SDL_CreateGPUDevice` selects Vulkan when a real
  ICD is present, confirmed via the CMake configure log's "GPU drivers: vulkan" line) — issue #151's own scope
  never required a macOS/Windows machine with real Metal/D3D12 hardware to actually run it, only that the code
  itself stays platform-generic (which it is — `SDL_GPU` abstracts the backend choice entirely; nothing in
  `sdl3_frame_backend.cpp` branches on platform). Whether CI's existing macOS/Windows release matrix jobs ever
  gain a `-DATLAS_RENDER_BACKEND=SDL3` leg (and, if so, whether their runners have real GPU hardware or need the
  same headless-CI skip path this library's tests already implement) is left for whoever picks up #69's next
  slice.
- **CI's `build-and-test` matrix does not yet install the extra OS-level X11 dev packages
  `ATLAS_RENDER_BACKEND=SDL3` needs on Linux, nor build that configuration** (see "Scoping decisions" above) —
  the default `NULL` build never needed them and remains completely unaffected, so this was deliberately not
  added speculatively; it becomes necessary the moment CI's actual build-and-test coverage is asked to exercise
  the `SDL3` configuration end to end (including running its tests, not just compiling it). The
  `static-analysis` job's own `compile_commands.json` build is a narrower case and does already install those
  packages and configure with `-DATLAS_RENDER_BACKEND=SDL3` — clang-tidy needs real compile flags for every
  changed file regardless of which backend gates it, not just the ones the default build compiles.
- **`Sdl3FrameBackend`'s window is currently fixed at construction (title/size/flags via constructor
  parameters only)** — no resize handling, no re-claiming the window if the swapchain becomes invalid (e.g. a
  monitor is unplugged), no multi-window support. Issue #151's scope was window+device bring-up and a
  clear-and-present loop only; a real render-loop host composing this backend (not built yet — #153/#154 and
  beyond) will need to decide how much of that becomes this library's concern versus the host's own.

## Provides

3D rendering: consumes composed properties and resources (game state), produces frame output. Serves as one
possible backend for the UI renderer contract (§19), never the mandatory one — backend selection remains a
host composition/deployment concern. `FrameBackend` (issue #148) formalizes that same "never mandatory" stance
one level down: which concrete `FrameBackend` a build compiles in is itself a compile-time choice, with
`NullFrameBackend` always available as the zero-dependency default and `Sdl3FrameBackend` (issue #151) now the
first real, opt-in one.

**Spec:** [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities)
(responsibility, now amended to include `atlas-render`), [§19 UI System](../../docs/specification/19-ui-system.md#backend-implementations)
(the State → Renderer → Output pattern this library implements, and the Backend Implementations section
naming it as one possible 3D backend), [§24 Non-Goals](../../docs/specification/24-non-goals.md) (amended:
rendering *technology* is no longer fully out of scope now that this optional reference implementation exists,
but content authoring — art direction, shader authoring — remains an application concern), [§5 Dependency Model](../../docs/specification/05-dependency-model.md)
(dependency rules), [§4 Architectural Invariants](../../docs/specification/04-architectural-invariants.md)
(determinism constraints — the reasoning behind `nlerp` over `slerp`, and behind `tick` never being read from a
clock inside this library), [§20 Property and Resource Composition](../../docs/specification/20-property-and-resource-composition.md)
(the composed properties/resources `build_frame` consumes as input state).

## Dependency position

`atlas-render` depends publicly on `atlas::core` (for `atlas::core::Time` — `Frame::tick`), `atlas::entity`
(for `atlas::EntityRef`), `atlas::resource` (for `atlas::ResourceId`), and `atlas::runtime` (for
`atlas::runtime::PropertyStore<T>`), plus `atlas_project_options`/`atlas_project_warnings` and the standard
library. `atlas::core` is linked directly rather than relied on transitively through `atlas::runtime` —
`atlas-runtime`'s own `CMakeLists.txt` does not link `atlas::core` today (nothing in its current public
interface needs `Time`), so `atlas-render` depends on it explicitly since its own public interface (`Frame`)
does.

Per §5, `atlas-render` is optional: a headless server host never gains a dependency on it, the same as
`atlas-input`, `atlas-ui`, and `atlas-editor`. It sits at the top of the runtime-library stack alongside
`atlas-runtime`, consuming the systems beneath it (entity identity, resource identity, property storage)
rather than being consumed by them — no capability, application, or editor/deployment-specific code is ever a
dependency of this library, consistent with the dependency direction §5 requires throughout.

`SDL3::SDL3-static` is an additional public dependency, but only when configured with
`-DATLAS_RENDER_BACKEND=SDL3` — the default `NULL` configuration never fetches, compiles, or links it, keeping
every other consumer of `atlas::render` (including any headless host) completely unaffected by this backend's
existence. This is the first genuinely new third-party dependency this project has taken on beyond
yaml-cpp/GoogleTest (`tools/atlas-cgen`, `tests/CMakeLists.txt`). Issue #153 adds `SDL3_shadercross::SDL3_shadercross-static`
alongside it, same `ATLAS_RENDER_BACKEND=SDL3`-only gating, plus two dependencies that never become link-time
dependencies of `atlas-render` itself: Microsoft's prebuilt DirectXShaderCompiler binaries (consumed by
`SDL_shadercross`'s own build, not linked into `atlas-render` directly) and a from-source SPIRV-Cross build
(same) — see "Scoping decisions" above for why both are fetched/built the way they are.

Issue #174 adds `atlas::windowing` alongside `SDL3::SDL3-static`, same `ATLAS_RENDER_BACKEND=SDL3`-only
gating — a downward dependency (`atlas-windowing` sits below both `atlas-render` and `atlas-input`, per
§5/§13), never a sideways one onto `atlas-input` itself.
