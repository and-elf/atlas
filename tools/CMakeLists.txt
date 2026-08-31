add_subdirectory(atlas-cgen)
add_subdirectory(atlas-rcc)
# Must come after atlas-cgen: atlas-depval links directly against
# atlas::cgen_lib (see tools/atlas-depval/CMakeLists.txt), so that target
# must already exist by the time this subdirectory's target_link_libraries
# call runs.
add_subdirectory(atlas-depval)
add_subdirectory(atlas-refl)
add_subdirectory(atlas-docgen)
add_subdirectory(atlas-adl)
