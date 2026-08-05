// This translation unit's only purpose is to #include the atlas-cgen tool's
// actual output (generated at build time from
// fixtures/intent_example.capability.yaml, see tests/atlas-cgen/CMakeLists.txt)
// so its static_asserts run for real, proving an IntentId-typed field
// actually compiles and satisfies atlas::PropertyContract - not just that
// map_field_type/required_include_for_type return the right strings. Never
// executed, so it's excluded from the coverage gate (cmake/CodeCoverage.cmake)
// rather than reported as permanently-uncovered code.
#include "intent_example.capability.hpp"
