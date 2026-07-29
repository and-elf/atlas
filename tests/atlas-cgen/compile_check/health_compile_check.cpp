// This translation unit's only purpose is to #include the atlas-cgen
// tool's actual output (generated at build time from
// fixtures/health.capability.yaml, see tests/atlas-cgen/CMakeLists.txt)
// so its static_asserts run for real. If atlas-cgen ever emitted a
// Health/ApplyDamage/HealthChanged that didn't satisfy
// atlas::PropertyContract/RequestContract/EventContract, this target - not
// a unit test - would fail to build. Never executed, so it's excluded from
// the coverage gate (cmake/CodeCoverage.cmake) rather than reported as
// permanently-uncovered code.
#include "health.capability.hpp"
