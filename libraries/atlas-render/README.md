# atlas-render

**Status:** Seeded (issue #30), plus a formal backend contract and null backend (issue #148), plus a minimal
mesh/texture asset format and decoder (issue #152), plus the first real backend — SDL3/SDL_GPU window + device
bring-up (issue #151, the first slice of #69). Implements the
`State → Renderer → Output` pattern (§19) for 3D rendering: `atlas::render::build_frame`
(`include/atlas/render/frame_builder.hpp`, `src/frame_builder.cpp`) consumes composed `Transform`/`Renderable`
property state — via the real `atlas::runtime::PropertyStore<T>`, not a stub — for an explicitly ordered set of
entities, and produces an `atlas::render::Frame`: an in-memory, testable list of `DrawCommand`s
(`include/atlas/render/frame.hpp`). The compile-time contract every backend (real or null) must satisfy, the
always-available `NullFrameBackend`, and now `Sdl3FrameBackend` — a real SDL3 window plus an `SDL_GPU` device
running a clear-and-present loop — all exist; see "What's implemented" and Scoping decisions below for exactly
what `Sdl3FrameBackend` does and doesn't do yet (no real geometry/shaders — that's #153/#154's job).

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
- **`atlas::render::Sdl3FrameBackend`** (`include/atlas/render/sdl3_frame_backend.hpp`,
  `src/sdl3_frame_backend.cpp`, issue #151, only compiled when `ATLAS_RENDER_BACKEND=SDL3`) — the first real
  (non-null) `FrameBackend`: constructs an SDL3 window and an `SDL_GPU` device, and claims the window for that
  device (swapchain setup). `submit(const Frame&)` acquires the swapchain texture, clears it to a fixed color,
  and presents it — `Frame::draw_commands` is entirely ignored this round (real geometry/shaders are #153/#154's
  job, not this one); only `frame.tick` is recorded. `last_completed_tick()` is driven by a real `SDL_GPUFence`
  per submission, polled and released as fences signal — never `NullFrameBackend`'s "instantly complete"
  shortcut, since tracking genuine GPU completion is the entire point of a real backend implementing this
  signal. An encapsulated class (not a basic aggregate, unlike this library's other types): it owns real
  OS/GPU resources with a genuine invariant to protect (every acquired handle released exactly once, in the
  right order), the same exception to Rule of Zero CLAUDE.md carves out for `atlas::entity::EntityRegistry`.
  Construction can fail (no GPU/display hardware, no supported `SDL_GPU` backend — the common case on CI
  runners) and reports that by throwing `std::runtime_error`, per CLAUDE.md's documented `std::expected`
  incompatibility — see "Scoping decisions" below (the headless-CI paragraph) for how this library's own tests
  handle that.

## Scoping decisions

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

**No real GPU/windowing backend before this round — deliberately deferred, per issue #30's explicit scope. This
round (#151) is the first slice; real geometry/shaders remain out of scope, per #153/#154.** Before #151, no new
third-party dependency (no SDL/Vulkan/bgfx/sokol) had been introduced. `build_frame` proves the actual mechanism —
consuming real composed property/resource state and producing a deterministic, testable frame descriptor —
without needing a display, a window, or a GPU context. Wiring `Frame`'s `DrawCommand` list into an actual
rasterizer, a windowing/surface layer, and a real asset resolver (today `atlas::ResourceId` is identity only;
`atlas-resource`'s own README defers resolution) is a distinct, larger follow-up: it needs a third-party
rendering dependency this issue was explicitly scoped not to introduce, and a resolver this repository doesn't
have yet either. Everything in this round is real machinery up to that boundary, not a stand-in for it.

**A null `ResourceId` is this round's answer to "a resource reference that doesn't resolve."** `atlas-resource`
implements identity only, with no resolver yet (see its own README), so there is no real "resolution failure"
this library can observe today. The null id (`ResourceId{}`, `is_null()`) is the one resource state already
meaningful without a resolver — an entity whose `Renderable::mesh` or `::material` is null is treated exactly
like an unresolved reference and skipped, not substituted with a placeholder draw. This is the closest faithful
analog available now; a real resolver's own failure mode (name not found, load error) is a future increment
this function will need to grow into once that resolver exists.

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

- **No skeletal animation, texture compression/mipmaps, or richer material data — issue #152's explicit
  scope.** `Vertex` is position/normal/UV only (no bone indices/weights, tracked separately per #45/#112);
  `DecodedTexture` is a single uncompressed RGBA8 layer (no mip chain, no PBR parameter set, no multiple
  texture slots). A first round needs just enough to draw one textured mesh — richer data is a real design
  question for whoever picks this back up, not silently precluded by anything in this format.
- **No production import pipeline (Assimp or similar) — deliberately out of scope.** This is a from-scratch
  minimal format for this project's own authoring/build pipeline, not a general importer for arbitrary
  externally-authored assets. Getting real content into this format (e.g. a Blender export step, or a small
  offline converter from glTF) is unaddressed here and left to whoever builds the authoring-side tooling.
- **No actual GPU upload exists yet.** `DecodedMesh`/`DecodedTexture` are CPU-side, in-memory, GPU-upload-ready
  data — the actual vertex/index buffer and texture creation calls against a real graphics API are issue #154's
  job, not this one's; nothing here has been exercised against a real GPU.
- No real resolver exists yet for `ResourceId` (§13, `atlas-resource`'s own scoping note), so `build_frame`'s
  "null id means unresolved" rule is the best available analog, not the final word: once a resolver exists, a
  *resolvable-but-failed* reference (name not found, asset load error) is a distinct case this function will
  need to grow a real answer for, separate from "the property was simply never set to anything." Whether that
  distinction becomes a new `build_frame` parameter, a richer return type, or a concern the eventual resolver
  itself absorbs is left for whoever designs the first real `atlas-render` backend to decide.
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
yaml-cpp/GoogleTest (`tools/atlas-cgen`, `tests/CMakeLists.txt`).
