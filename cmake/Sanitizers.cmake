# AddressSanitizer + UndefinedBehaviorSanitizer for debug/CI builds. Not
# applied by default (ATLAS_ENABLE_SANITIZERS, off by default) since it is
# not meaningful for MSVC in this configuration and roughly doubles build
# time; enabled explicitly by the CI "sanitized" build jobs.
function(atlas_enable_sanitizers target_name)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(WARNING "ATLAS_ENABLE_SANITIZERS is ON but sanitizers are only wired up for GCC/Clang")
    return()
  endif()

  target_compile_options(${target_name} INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(${target_name} INTERFACE -fsanitize=address,undefined)
endfunction()
