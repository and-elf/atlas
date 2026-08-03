# Issue #178: atlas-physics gains its first non-header-only content
# (src/jolt_physics_backend.cpp, only compiled when ATLAS_PHYSICS_BACKEND=JOLT
# below) - a regular library target from now on, never INTERFACE again, even
# though the NULL backend still has nothing but header-only content of its
# own. CMake errors at generate time ("No SOURCES given to target") for a
# STATIC/SHARED library target with zero source files (verified directly,
# not assumed), so src/atlas_physics.cpp exists purely to be that one
# always-present source file - see its own doc comment. This keeps
# atlas-physics a single target that is always a regular library regardless
# of backend, the same shape atlas-render's own CMakeLists.txt already uses
# (its atlas-render target always has real sources - src/frame_builder.cpp
# etc. - so it never had this problem in the first place) - rather than a
# target that is INTERFACE under one backend and STATIC under another, which
# isn't a pattern this codebase uses elsewhere for a single target.
add_library(atlas-physics src/atlas_physics.cpp)
add_library(atlas::physics ALIAS atlas-physics)

target_include_directories(atlas-physics PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                 $<INSTALL_INTERFACE:include>)

target_link_libraries(atlas-physics PUBLIC atlas_project_options atlas::core
                       PRIVATE atlas_project_warnings)

# Backend selection (issue #177/#178): which concrete
# atlas::physics::PhysicsBackend a build compiles in, resolved at configure
# time (spec §4: no runtime discovery) - never a runtime factory/plugin
# lookup, mirroring ATLAS_RENDER_BACKEND/ATLAS_AUDIO_BACKEND exactly.
# NullPhysicsBackend (include/atlas/physics/null_physics_backend.hpp) is
# header-only and always available regardless of this option; "JOLT" (issue
# #178) is the first real backend - FetchContent'd Jolt Physics, a genuine
# JPH::PhysicsSystem, real gravity-driven body simulation behind one
# hardcoded placeholder shape (see libraries/atlas-physics/README.md).
# Selecting anything else fails the configure step with a clear message
# rather than silently building nothing.
set(ATLAS_PHYSICS_BACKEND
    "NULL"
    CACHE STRING "atlas-physics backend to build: NULL (always available, no rigid-body simulation) \
or JOLT (issue #178 - real Jolt Physics rigid-body simulation)")
set_property(CACHE ATLAS_PHYSICS_BACKEND PROPERTY STRINGS "NULL" "JOLT")

if(ATLAS_PHYSICS_BACKEND STREQUAL "JOLT")
  include(FetchContent)

  # --- Jolt CMake option investigation (issue #178) -------------------------
  #
  # Every option below was checked against Jolt's own Build/CMakeLists.txt
  # and Jolt/Jolt.cmake (v5.6.0) rather than copied from
  # JoltPhysicsHelloWorld's own reference FetchContent block unchanged - see
  # this library's README, "Jolt CMake integration (issue #178)", for the
  # full write-up. Summary of the two choices that differ from that
  # reference, or needed real verification:
  #
  # CROSS_PLATFORM_DETERMINISTIC: ON, not OFF (the HelloWorld reference's own
  # value) - non-negotiable given spec §4 and docs/specification/24-non-goals.md's
  # amendment that atlas-physics stays inside the determinism boundary.
  # Jolt's own Docs/Architecture.md ("Deterministic Simulation") confirms
  # this is the documented, supported way to get bit-exact cross-platform
  # results (~8% slower; verified this project's own sanitized debug preset
  # still configures/builds/links cleanly with it ON - see the README).
  # Also disables FMA contraction (Jolt/Jolt.cmake: "FMA is not compatible
  # with cross platform determinism"), which is exactly this project's own
  # -ffp-contract=off requirement (root CMakeLists.txt) applied to Jolt's own
  # code.
  #
  # USE_SSE4_1/USE_SSE4_2/USE_AVX/USE_AVX2/USE_AVX512: left ON (AVX512 OFF),
  # matching the reference, EVEN THOUGH these are x86-specific SIMD features
  # and this project also targets macOS/ARM64 (CLAUDE.md) - verified Jolt's
  # own Jolt/Jolt.cmake already guards the actual compiler flags
  # (-msse4.1/-mavx2/etc.) and the JPH_USE_* preprocessor defines behind its
  # own CMAKE_SYSTEM_PROCESSOR/CMAKE_OSX_ARCHITECTURES check ("ARM64 uses no
  # special commandline flags", Jolt/Jolt.cmake) - so these cache variables
  # are silently inert on an ARM64 configure and need no extra guarding on
  # our side. Not independently re-verified in *this* sandbox (x86-64 only);
  # logically correct per Jolt's own CMake source, flagged here as untested
  # on the ARM path in this environment specifically.
  #
  # OVERRIDE_CXX_FLAGS: ON, matching the reference - verified this does NOT
  # leak into atlas's own compiler flags. Build/CMakeLists.txt sets
  # CMAKE_CXX_FLAGS/CMAKE_CXX_FLAGS_DEBUG/CMAKE_CXX_FLAGS_RELEASE via plain
  # (non-CACHE, non-PARENT_SCOPE) set() calls - CMake directory-variable
  # scoping means that only affects the directory FetchContent adds Jolt's
  # own Build/CMakeLists.txt as (and Jolt's own sub-targets within it, none
  # of which get built here since TARGET_UNIT_TESTS/TARGET_HELLO_WORLD/etc.
  # below are off), never propagating up to this file's own directory scope
  # where atlas-physics/atlas_project_options/atlas_project_warnings are
  # defined. Confirmed empirically: atlas_project_warnings' own -Werror and
  # this project's global -ffp-contract=off both still apply, unaffected,
  # to every atlas:: target after this FetchContent block runs.
  #
  # CPP_EXCEPTIONS_ENABLED: OFF, matching the reference - this is a per-TU
  # compilation choice for Jolt's own sources, not a whole-program ABI
  # switch. JoltPhysicsBackend's own code (this library's
  # src/jolt_physics_backend.cpp, compiled as an ordinary atlas-physics TU
  # with exceptions enabled as normal) never needs an exception to unwind
  # through a Jolt-compiled frame - it only calls into Jolt's own C++ API
  # from outside, and Jolt's public API reports failure via return values
  # (e.g. an invalid JPH::BodyID), never a thrown exception. No observed
  # link/ABI issue building and running this library's own tests.
  #
  # Threading: JobSystemSingleThreaded (not JobSystemThreadPool) - see the
  # README's own "Threading" section for why.
  set(DOUBLE_PRECISION
      OFF
      CACHE BOOL "" FORCE)
  set(GENERATE_DEBUG_SYMBOLS
      ON
      CACHE BOOL "" FORCE)
  set(OVERRIDE_CXX_FLAGS
      ON
      CACHE BOOL "" FORCE)
  set(CROSS_PLATFORM_DETERMINISTIC
      ON
      CACHE BOOL "" FORCE)
  set(INTERPROCEDURAL_OPTIMIZATION
      ON
      CACHE BOOL "" FORCE)
  set(FLOATING_POINT_EXCEPTIONS_ENABLED
      OFF
      CACHE BOOL "" FORCE)
  set(CPP_EXCEPTIONS_ENABLED
      OFF
      CACHE BOOL "" FORCE)
  # ON, NOT the HelloWorld reference's own OFF value - discovered empirically,
  # not assumed: with CPP_RTTI_ENABLED OFF (Jolt's own default), linking this
  # library's own JoltPhysicsBackend translation unit (compiled with this
  # project's own ambient RTTI-enabled default - nothing in this project
  # passes -fno-rtti anywhere) against Jolt's own compiled objects failed with
  # "undefined reference to typeinfo for JPH::Shape/JPH::ConvexShape/
  # JPH::SphereShape/JPH::JobSystemSingleThreaded/..." - a real, reproduced
  # link error, not a hypothetical one. GCC only emits a class's typeinfo
  # object from the translation unit that defines its virtual "key function"
  # when that TU is itself compiled with RTTI enabled; Jolt's own .cpp files
  # (compiled -fno-rtti per its own default) never emit it, leaving this
  # project's own RTTI-enabled code - which touches these polymorphic Jolt
  # types (JPH::SphereShape via `new`, JPH::JobSystemSingleThreaded as a
  # member) - with an unresolvable external reference. Enabling RTTI for
  # Jolt's own build matches it to this project's own ambient default rather
  # than punching a one-off -fno-rtti hole in a single atlas-physics
  # translation unit, and costs only a documented, small (~5% MSVC, ~0% clang
  # per Jolt's own release notes) simulation-speed difference - Jolt itself
  # never uses C++ RTTI (`typeid`/`dynamic_cast`) internally; its own
  # from-scratch RTTI-like reflection system (JPH_DECLARE_RTTI_VIRTUAL,
  # Factory) is unrelated to this flag.
  set(CPP_RTTI_ENABLED
      ON
      CACHE BOOL "" FORCE)
  set(OBJECT_LAYER_BITS
      16
      CACHE STRING "" FORCE)
  set(USE_SSE4_1
      ON
      CACHE BOOL "" FORCE)
  set(USE_SSE4_2
      ON
      CACHE BOOL "" FORCE)
  set(USE_AVX
      ON
      CACHE BOOL "" FORCE)
  set(USE_AVX2
      ON
      CACHE BOOL "" FORCE)
  set(USE_AVX512
      OFF
      CACHE BOOL "" FORCE)

  # Never build Jolt's own executables (UnitTests/HelloWorld/PerformanceTest/
  # Samples/JoltViewer) - we only want the Jolt library itself. Belt-and-
  # braces: Build/CMakeLists.txt already gates all of these behind
  # `CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR`, which is false when
  # Jolt is FetchContent'd as a subdirectory of this project, so none of them
  # would be built regardless - these explicit OFFs just make that
  # non-reliance on Jolt's own top-level-only guard explicit, in case a
  # future Jolt release restructures that guard.
  set(TARGET_UNIT_TESTS
      OFF
      CACHE BOOL "" FORCE)
  set(TARGET_HELLO_WORLD
      OFF
      CACHE BOOL "" FORCE)
  set(TARGET_PERFORMANCE_TEST
      OFF
      CACHE BOOL "" FORCE)
  set(TARGET_SAMPLES
      OFF
      CACHE BOOL "" FORCE)
  set(TARGET_VIEWER
      OFF
      CACHE BOOL "" FORCE)

  # SYSTEM (CMake >= 3.25, our minimum) - see tools/atlas-cgen/CMakeLists.txt's
  # yaml-cpp FetchContent_Declare for why this matters, not just tidiness:
  # without it, a warning purely inside Jolt's own headers can still fail our
  # -Werror via whichever of our files includes them. SOURCE_SUBDIR "Build"
  # is required - Jolt's own top-level CMakeLists.txt lives at Build/
  # CMakeLists.txt, not the repository root (verified directly against the
  # fetched checkout, not assumed).
  FetchContent_Declare(
    JoltPhysics SYSTEM
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics
    GIT_TAG v5.6.0
    SOURCE_SUBDIR "Build")
  FetchContent_MakeAvailable(JoltPhysics)

  # Third-party sources are out of scope for our clang-tidy gate
  # (CMAKE_CXX_CLANG_TIDY is a global default applied to every target,
  # including fetched dependencies) - mirrors atlas-render's own SDL3
  # precedent exactly.
  if(TARGET Jolt)
    set_target_properties(Jolt PROPERTIES CXX_CLANG_TIDY "")
  endif()

  target_sources(atlas-physics PRIVATE src/jolt_physics_backend.cpp)
  target_link_libraries(atlas-physics PUBLIC Jolt::Jolt)
elseif(NOT ATLAS_PHYSICS_BACKEND STREQUAL "NULL")
  message(
    FATAL_ERROR
      "ATLAS_PHYSICS_BACKEND='${ATLAS_PHYSICS_BACKEND}' is not a supported value - only NULL and "
      "JOLT exist today")
endif()
