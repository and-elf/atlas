// This translation unit's only purpose is to #include the atlas-cgen
// tool's actual host-composition output (generated at build time from
// fixtures/gameplay_client.host.yaml composing health+armor, see
// tests/atlas-cgen/CMakeLists.txt) so its generated struct and
// register_property_stores function body are actually compiled - not just
// asserted against generated text. If atlas-cgen ever emitted a
// GameplayClient struct, or a register_property_stores whose body, didn't
// actually compile against real atlas::runtime::PropertyStore/Context and
// the health/armor capabilities' own generated contracts, this target -
// not a unit test - would fail to build. Never executed, so it's excluded
// from the coverage gate (cmake/CodeCoverage.cmake) rather than reported
// as permanently-uncovered code - runtime proof that composing capabilities
// this way actually works is demo/tests/simulated_host.hpp's job, not
// this tool's own test suite's (see this tool's README).
#include "gameplay_client.host.hpp"
