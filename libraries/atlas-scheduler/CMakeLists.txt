add_library(atlas-scheduler src/scheduler.cpp)
add_library(atlas::scheduler ALIAS atlas-scheduler)

target_include_directories(atlas-scheduler PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                   $<INSTALL_INTERFACE:include>)

# run_tick_parallel() (std::async/std::future) needs the platform threading
# library linked explicitly - libstdc++/libc++ do not pull it in implicitly
# on Linux.
find_package(Threads REQUIRED)

target_link_libraries(
  atlas-scheduler
  PUBLIC atlas_project_options atlas::stage Threads::Threads
  PRIVATE atlas_project_warnings)
