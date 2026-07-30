cmake_minimum_required(VERSION 3.25)

project(
  Atlas
  VERSION 0.1.0
  DESCRIPTION "Compile-time composed, server-authoritative platform for real-time interactive applications"
  LANGUAGES CXX
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE
      "Debug"
      CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(ATLAS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(ATLAS_ENABLE_SANITIZERS "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(ATLAS_ENABLE_COVERAGE "Instrument the build for code coverage" OFF)
option(ATLAS_ENABLE_CLANG_TIDY "Run clang-tidy as part of the build" OFF)
option(ATLAS_BUILD_TESTS "Build the test suite" ON)
option(ATLAS_BUILD_TOOLS "Build developer tools (manifest-to-contract generator, etc.)" ON)

include(CompilerWarnings)
include(Sanitizers)
include(CodeCoverage)
include(StaticAnalysis)
include(GenerateCapabilityContract)
include(GenerateHostComposition)

if(ATLAS_ENABLE_CLANG_TIDY)
  atlas_enable_clang_tidy()
endif()

# Interface targets every library/test target links against, rather than
# setting compiler flags globally. Keeps per-target opt-out possible and
# keeps the flags themselves defined in exactly one place (cmake/*.cmake).
add_library(atlas_project_options INTERFACE)
add_library(atlas_project_warnings INTERFACE)

target_compile_features(atlas_project_options INTERFACE cxx_std_23)

# Bit-exact determinism (spec §4) is a platform guarantee, not an
# optimization target: floating-point contraction (fused multiply-add)
# must not silently change results between compilers/targets, so this is
# pinned explicitly rather than left at the compiler's default. Never add
# -ffast-math (or /fp:fast) to any target in this project.
target_compile_options(
  atlas_project_options
  INTERFACE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-ffp-contract=off>)

atlas_set_project_warnings(atlas_project_warnings ${ATLAS_WARNINGS_AS_ERRORS})

if(ATLAS_ENABLE_SANITIZERS)
  atlas_enable_sanitizers(atlas_project_options)
endif()

if(ATLAS_ENABLE_COVERAGE)
  atlas_enable_coverage(atlas_project_options)
endif()

add_subdirectory(libraries)

if(ATLAS_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if(ATLAS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)

  # demo/ is a growing gameplay demo built on Atlas, staged in this repo for
  # now (see demo/README.md) - gated alongside tests/ since its own tests
  # need GTest, fetched by tests/CMakeLists.txt above.
  add_subdirectory(demo)
endif()
