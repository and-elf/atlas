// This translation unit's only purpose is to #include both atlas-cgen's
// and atlas-refl's actual output for the same fixture manifest
// (tests/fixtures/health.capability.yaml, generated at build time - see
// tests/atlas-refl/CMakeLists.txt) so the static_asserts below prove
// atlas-refl's generated reflection metadata is genuinely consumable
// alongside atlas-reflection's existing runtime primitives
// (field_count()/field_types_t<T>) - not a parallel, unrelated shape that
// happens to compile on its own. If atlas-refl ever emitted metadata whose
// field count didn't match the real generated contract struct it describes,
// this target - not a unit test - would fail to build. Never executed at
// runtime, so it's excluded from the coverage gate the same way
// tools/atlas-cgen/README.md documents for its own compile_check targets
// (cmake/CodeCoverage.cmake's tests/.* exclude already covers everything
// under tests/, including this file).
#include "atlas/reflection/field_count.hpp"
#include "atlas/reflection/field_visitor.hpp"

#include <tuple>

#include "health.capability.hpp"            // atlas-cgen's generated contract: atlas::health::Health, etc.
#include "health.capability.reflection.hpp" // atlas-refl's generated metadata: atlas::refl::health::kHealthFields, etc.

namespace {

// Health has 2 fields (current, maximum) - both the real generated contract
// struct (field_count(), a genuine compile-time property recovered purely
// from the struct's own shape) and atlas-refl's generated metadata (an
// array built from the manifest, independently, at generation time) must
// agree on that count, or the two generators have silently drifted apart.
static_assert(atlas::reflection::field_count<atlas::health::Health>() ==
                  atlas::refl::health::kHealthFields.size(),
              "atlas-refl's field metadata must describe exactly the fields "
              "atlas-reflection can walk on the real generated contract struct");

static_assert(std::tuple_size_v<atlas::reflection::field_types_t<atlas::health::Health>> ==
                  atlas::refl::health::kHealthFields.size(),
              "field_types_t<Health>'s field count must match atlas-refl's generated metadata count");

static_assert(atlas::refl::health::kHealthFields[0].name == "current");
static_assert(atlas::refl::health::kHealthFields[0].type_name == "std::int32_t");
static_assert(atlas::refl::health::kHealthFields[1].name == "maximum");
static_assert(atlas::refl::health::kHealthFields[1].type_name == "std::int32_t");

// ApplyDamage (a request, not a property) gets the same proof - reflection
// metadata isn't property-specific.
static_assert(atlas::reflection::field_count<atlas::health::ApplyDamage>() ==
              atlas::refl::health::kApplyDamageFields.size());
static_assert(atlas::refl::health::kApplyDamageFields[0].name == "target");
static_assert(atlas::refl::health::kApplyDamageFields[1].name == "amount");

// HealthChanged (an event) too.
static_assert(atlas::reflection::field_count<atlas::health::HealthChanged>() ==
              atlas::refl::health::kHealthChangedFields.size());

} // namespace
