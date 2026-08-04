add_library(atlas-replication src/entity_ref_codec.cpp src/resource_id_codec.cpp src/property_id_codec.cpp)
add_library(atlas::replication ALIAS atlas-replication)

target_include_directories(atlas-replication PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                      $<INSTALL_INTERFACE:include>)

target_link_libraries(
  atlas-replication
  PUBLIC atlas_project_options atlas::entity atlas::resource atlas::serialization atlas::reflection
  PRIVATE atlas_project_warnings)

# Transport backend selection (issue #218): which concrete
# atlas::replication::Transport a build compiles in, resolved at configure
# time (spec §4: no runtime discovery) - never a runtime factory/plugin
# lookup, mirroring ATLAS_PHYSICS_BACKEND/ATLAS_AUDIO_BACKEND/
# ATLAS_RENDER_BACKEND exactly. NullTransport (include/atlas/replication/
# null_transport.hpp) is header-only and always available regardless of this
# option; "UNIX" (issue #218) is the first real backend - AF_UNIX/SOCK_DGRAM
# Unix domain sockets, POSIX-only (see UnixSocketTransport's own header doc
# comment on Windows). Selecting anything else fails the configure step with
# a clear message rather than silently building nothing. #216 explicitly
# deferred introducing this option until a second, real backend existed to
# select between - this is that point.
set(ATLAS_REPLICATION_TRANSPORT
    "NULL"
    CACHE STRING "atlas-replication transport backend to build: NULL (always available, no real \
transport) or UNIX (issue #218 - AF_UNIX/SOCK_DGRAM Unix domain sockets, POSIX only)")
set_property(CACHE ATLAS_REPLICATION_TRANSPORT PROPERTY STRINGS "NULL" "UNIX")

if(ATLAS_REPLICATION_TRANSPORT STREQUAL "UNIX")
  target_sources(atlas-replication PRIVATE src/unix_socket_transport.cpp)
elseif(NOT ATLAS_REPLICATION_TRANSPORT STREQUAL "NULL")
  message(
    FATAL_ERROR
      "ATLAS_REPLICATION_TRANSPORT='${ATLAS_REPLICATION_TRANSPORT}' is not a supported value - only "
      "NULL and UNIX exist today")
endif()
