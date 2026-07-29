include(FetchContent)

set(YAML_CPP_BUILD_TESTS
    OFF
    CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS
    OFF
    CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB
    OFF
    CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL
    OFF
    CACHE BOOL "" FORCE)

# SYSTEM (CMake >= 3.25, our minimum) marks yaml-cpp's headers -isystem for
# consumers - without it, a warning that originates purely inside yaml-cpp's
# own headers (e.g. its enum members shadowing each other, -Wshadow) still
# gets attributed to whichever of OUR files #included yaml-cpp and fails our
# -Werror. HeaderFilterRegex in .clang-tidy only suppresses clang-tidy's own
# check diagnostics for third-party headers, not plain compiler diagnostics
# clang-tidy also surfaces - SYSTEM is what actually silences those.
FetchContent_Declare(
  yaml-cpp SYSTEM
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0)
FetchContent_MakeAvailable(yaml-cpp)

# Third-party sources are out of scope for our clang-tidy gate (CMAKE_CXX_CLANG_TIDY
# is a global default applied to every target, including fetched dependencies).
if(TARGET yaml-cpp)
  set_target_properties(yaml-cpp PROPERTIES CXX_CLANG_TIDY "")
endif()

# Template files (templates/*.tmpl) are embedded as constexpr string_view
# constants at build time (cmake/scripts/EmbedTextFile.cmake) rather than
# read from disk at runtime - see that script for the full rationale. Each
# add_custom_command's DEPENDS on the .tmpl file itself makes editing a
# template an ordinary incremental-rebuild trigger, same as any generated
# source.
set(ATLAS_CONTRACT_GEN_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_templates")

function(atlas_contract_gen_embed_template template_name)
  add_custom_command(
    OUTPUT "${ATLAS_CONTRACT_GEN_GENERATED_DIR}/${template_name}_template.hpp"
    COMMAND
      ${CMAKE_COMMAND} -DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/templates/${template_name}.tmpl
      -DOUTPUT_FILE=${ATLAS_CONTRACT_GEN_GENERATED_DIR}/${template_name}_template.hpp
      -DCPP_NAME=${template_name} -P ${CMAKE_SOURCE_DIR}/cmake/scripts/EmbedTextFile.cmake
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/templates/${template_name}.tmpl"
            "${CMAKE_SOURCE_DIR}/cmake/scripts/EmbedTextFile.cmake"
    COMMENT "Embedding template ${template_name}.tmpl"
    VERBATIM)
endfunction()

atlas_contract_gen_embed_template(contract_file)
atlas_contract_gen_embed_template(struct_decl)

add_library(
  contract-gen-lib
  src/manifest.cpp
  src/template_engine.cpp
  src/contract_writer.cpp
  "${ATLAS_CONTRACT_GEN_GENERATED_DIR}/contract_file_template.hpp"
  "${ATLAS_CONTRACT_GEN_GENERATED_DIR}/struct_decl_template.hpp")
add_library(atlas::contract_gen_lib ALIAS contract-gen-lib)

target_include_directories(
  contract-gen-lib
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
  PRIVATE "${ATLAS_CONTRACT_GEN_GENERATED_DIR}")

target_link_libraries(contract-gen-lib PUBLIC atlas_project_options yaml-cpp::yaml-cpp
                       PRIVATE atlas_project_warnings)

add_executable(contract-gen src/main.cpp)
target_link_libraries(contract-gen PRIVATE atlas::contract_gen_lib atlas_project_warnings)
