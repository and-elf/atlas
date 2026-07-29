// This translation unit's only purpose is to #include the atlas-cgen
// tool's actual output (generated at build time from
// fixtures/armor.capability.yaml, see tests/atlas-cgen/CMakeLists.txt)
// so its static_asserts run for real, proving a composed (composition:
// Additive) property actually compiles and satisfies both
// atlas::PropertyContract and atlas::Composable - not just that
// map_composition_strategy/generate_contract return the right strings.
// Never executed, so it's excluded from the coverage gate
// (cmake/CodeCoverage.cmake) rather than reported as permanently-uncovered
// code.
#include "armor.capability.hpp"
