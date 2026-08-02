# atlas-render

**Status:** Seeded (issue #30), plus a formal backend contract and null backend (issue #148), plus a minimal
mesh/texture asset format and decoder (issue #152), plus the first real backend — SDL3/SDL_GPU window + device
bring-up (issue #151, the first slice of #69) — plus a real shader/pipeline/draw-call path proved with one
hardcoded triangle (issue #153) — plus, now, real content: `Sdl3FrameBackend::submit()` resolves each
`DrawCommand`'s mesh/material through two GPU-upload caches, builds a model matrix from its `Transform`, and
issues one real indexed draw per surviving command (issue #154), superseding and removing issue #153's
hardcoded-triangle scaffolding entirely. Implements the `State → Renderer → Output` pattern (§19) for 3D
rendering: `atlas::render::build_frame` (`include/atlas/render/frame_builder.hpp`, `src/frame_builder.cpp`)
consumes composed `Transform`/`Renderable` property state — via the real `atlas::runtime::PropertyStore<T>`, not
a stub — for an explicitly ordered set of entities, and produces an `atlas::render::Frame`: an in-memory,
testable list of `DrawCommand`s (`include/atlas/render/frame.hpp`). The compile-time contract every backend
(real or null) must satisfy, the always-available `NullFrameBackend`, `Sdl3FrameBackend` — a real SDL3 window
plus an `SDL_GPU` device now drawing real per-`DrawCommand` content every frame — all exist; see "What's
implemented" and "Scoping decisions" below for exactly what this round does and doesn't do yet (still no real
`Camera`/view-projection concept — see "Open questions").

## What's implemented

- **`atlas::render::Vec3`** and **`atlas::render::Quaternion`** (`include/atlas/render/transform.hpp`) —
  plain 3D vector and rotation value types. Basic aggregates (rule of zero): no invariant either type
  enforces itself.
- **`atlas::render::Transform`** (same header) — a renderable entity's full spatial state: `position`,
  `rotation`, `scale` (defaulting to unit scale). A basic aggregate.
- **`atlas::render::lerp`** (three overloads: `float`, `Vec3`, `Transform`) and **`atlas::render::nlerp`**
  (`Quaternion`) — presentation-only interpolation (spec §4: "Wall-clock time may be used only for
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
  of plain floats, reusing `Vec3` from `transform.hpp`). Returns `std::optional<DecodedMesh>` —
  `std::nullopt` for any malformed or truncated input — rather than throwing; see "Scoping decisions" below
  for the exact format layout and why it needs no third-party dependency.
- **`atlas::render::decode_texture`** (`include/atlas/render/texture_asset.hpp`, `src/texture_asset.cpp`,
  issue #152) — decodes raw bytes the same way, against this library's own minimal, hand-rolled raw/
  uncompressed RGBA8 texture format, into a GPU-upload-ready `atlas::render::DecodedTexture` (pixel bytes plus
  `width`/`height`). Also `std::optional`-returning, including an explicit overflow-safe rejection of
  adversarial width/height values rather than a naive multiplication that could wrap around — see "Scoping
  decisions" below.
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
  `atlas-audio::DecodeCache`'s own "registry must outlive this cache" contract. `submit(const Frame&)` acquires
  the swapchain texture, clears it, then — issue #154's actual point — resolves each `DrawCommand`'s
  `mesh`/`material` through the two caches, skips (never substitutes) any command whose mesh or texture failed
  to resolve/decode/upload, builds a model matrix from its `Transform` (`to_model_matrix`, above), and issues
  one real indexed draw per surviving command, in `Frame::draw_commands`'s own order, unconditionally (no
  culling — issue #156's separate job) — then presents. `last_completed_tick()` is driven by a real
  `SDL_GPUFence` per submission, polled and released as fences signal — never `NullFrameBackend`'s "instantly
  complete" shortcut. An encapsulated class (not a basic aggregate, unlike this library's other types): it owns
  real OS/GPU resources (window, device, mesh pipeline, both upload caches, in-flight fences) with a genuine
  invariant to protect (every acquired handle released exactly once, in the right order), the same exception to
  Rule of Zero CLAUDE.md carves out for `atlas::entity::EntityRegistry`. Construction can fail (no GPU/display
  hardware, no supported `SDL_GPU` backend, a shader/pipeline build failure — the common case on CI runners) and
  reports that by throwing `std::runtime_error`, per CLAUDE.md's documented `std::expected` incompatibility —
  see "Scoping decisions" below (the headless-CI paragraph) for how this library's own tests handle that.
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
  `decode_mesh` produces indices, and this function always uses them. `destroy_sdl3_mesh_pipeline()` releases
  both handles and is idempotent, managed explicitly by `Sdl3FrameBackend::destroy()` rather than its own RAII
  class, mirroring `Sdl3TrianglePipeline`'s own now-removed lifetime-ordering discipline.

## Scoping decisions

**No `Camera`/view-projection concept exists anywhere in Atlas yet, and issue #154 does not add one — locked in
before implementation, not re-litigated here.** Without a camera, a per-`DrawCommand` model matrix alone has
nowhere well-defined to land in clip space. This round's answer: apply each `DrawCommand`'s `Transform` as a
model matrix (`to_model_matrix`, `transform.hpp`) and push *only that* to the vertex shader — no separately
authored "identity view-projection" matrix multiplied in alongside it. Composing against an explicit identity
matrix would be mathematically a no-op but would falsely suggest a view-projection *concept* already exists in
this code that a future `Camera` type would need to slot into; leaving it out entirely is the more honest
expression of "this mechanism doesn't exist yet," and `mesh.vert.hlsl`'s own doc comment states this explicitly.
A real `Camera` type (view/projection, presentation-only per §4/§20) is flagged below under "Open questions" as
a genuine follow-up gap, not silently precluded by anything here.

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

**This round draws its own boundary at "the draw call executes without error" — it does not verify what actually
lands on screen.** Issue #155 (not started here) is the "spinning box" visual test this round deliberately
doesn't build; `tests/atlas-render/sdl3_frame_backend_test.cpp`'s
`Sdl3FrameBackendTest.SubmitDrawsARealResolvedDrawCommandWithoutThrowing` submits a real `Frame`/`DrawCommand`
against a real `ResourceRegistry` (built from real packed fixture blobs wrapping the existing
`triangle.mesh`/`checker.tex` fixtures) into the real window's swapchain and asserts only that it doesn't throw
and eventually reports completion — proving the full resolve → decode → GPU-upload → draw mechanism executes
cleanly end-to-end, not that the rendered pixels are a correctly textured triangle. Confirming actual pixel
output would need either a real GPU in this sandbox (unavailable — see the headless-CI paragraph below) or a
software/CPU Vulkan implementation (e.g. Lavapipe/SwiftShader), neither installed nor evaluated in this round.

**`Sdl3FrameBackend`'s own tests decide "no real GPU/display" via genuine construction failure, not a mocked
driver — the "Headless CI" open question this issue asked to be resolved, not silently skipped.** Concretely:
every test attempts *real* `SDL3`/`SDL_GPU` window and device creation; `SDL_HINT_VIDEO_DRIVER` is forced to
`"dummy"` first so windowing itself always succeeds headlessly (no `DISPLAY`, no real compositor needed) —
verified against this project's own sandboxed dev environment, which has no `/dev/dri`, no Vulkan ICD installed
at all (`libvulkan1`'s loader is present with nothing for it to load), so `SDL_CreateGPUDevice` reliably fails
there with `"No supported SDL_GPU backend found!"` after the window itself was created successfully. Each test
(a `Sdl3FrameBackendTest` GTest fixture, `tests/atlas-render/sdl3_frame_backend_test.cpp`) attempts real
construction in `SetUp()` and calls `GTEST_SKIP()` — logging the thrown exception's message, not swallowing it —
the moment that fails, rather than either mocking `SDL_GPU` out (which would stop testing the real API surface
entirely) or leaving the test suite red on every machine without a GPU. On a machine that *does* have a working
`SDL_GPU` backend, the exact same tests exercise the real success path (window/device creation, clear-and-present,
real fence completion) instead of skipping — this was deliberately not special-cased away from CI, so a
developer's local machine with real hardware gets full coverage for free. A separate, always-runnable test
(`Sdl3FrameBackendConstruction.FailureReportsSdlErrorTextInTheException`) forces a *deterministic* failure — an
invalid `SDL_HINT_VIDEO_DRIVER` value, not dependent on the sandbox's own missing GPU/Vulkan-ICD state — so the
"construction failure surfaces a real error message" behavior itself has coverage on every machine, GPU or not.

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

**`Vertex` reuses `Vec3` (`transform.hpp`) for `position`/`normal` rather than a duplicate 3-float type.** Both
are already plain, GPU-conventional-precision `float` triples with no invariant of their own — introducing a
second identical-shaped struct just for mesh data would be duplication without a distinguishing reason.

## Open questions (flagging for human review, not silently resolved)

- **No real `Camera`/view-projection concept exists anywhere in Atlas yet — issue #154's own locked-in scope
  deferred this deliberately, not silently.** `Sdl3FrameBackend` pushes each `DrawCommand`'s model matrix alone
  (`to_model_matrix`, `transform.hpp`) to the vertex shader, with nothing composed against it — there is
  currently no way to move, rotate, or project a viewpoint at all; every `DrawCommand` renders as if the camera
  sits at the origin looking down the shader's own implicit clip-space convention. A real `Camera` type (likely
  a composed property alongside `Transform`, presentation-only per §4/§20's "view/projection... presentation
  concern" framing) is a genuine, undesigned follow-up for whoever picks up #69's next slice — it needs its own
  scoping decision (a single active camera vs. multiple viewports, how it composes with split-screen or
  multi-window rendering) this round did not attempt to anticipate.
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
