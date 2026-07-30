#pragma once

// Generated at build time from demo/modules/health/health.capability.yaml
// (see demo/CMakeLists.txt) - the Health/ApplyDamage/HealthChanged contracts.
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"

#include <optional>

#include "health.capability.hpp"

namespace atlas::health {

// The manual implementation of ApplyDamage's request handler (spec §14,
// Manual Implementation; §21 worked example, reproduced with a real,
// type-checking C++23 optional chain rather than the worked example's own
// illustrative or_else/and_then pseudocode - see this directory's README
// for why). Reads the target's Armor *contract* (a tiny-interface
// dependency on another capability's property shape, spec §5) purely
// through ctx.get<Armor>() - this file never includes armor.hpp or calls
// into armor's implementation directly, so health and armor stay mutually
// unaware of each other exactly as spec §20's Design Rule requires: health
// consumes Armor's already-composed effective value and never needs to
// know it is composed at all, let alone by what.
[[nodiscard]] RequestResult on_apply_damage(Context& ctx, const ApplyDamage& cmd);

// Health's own wire encoding - this capability's manual implementation
// decides how its own property goes over the wire (spec §14), the same way
// it decides its own request-handling logic; a property's replication
// encoding is not something atlas-replication or atlas-serialization
// invents on a capability's behalf (neither library has any notion of
// "Health" and never will, spec §2). Built directly on
// atlas-serialization's write_i32/read_i32 - explicit little-endian,
// current then maximum - mirroring atlas-replication's own EntityRef/
// ResourceId codecs' precedent (§4: no memcpy of native layout).
void write_health(serialization::ByteWriter& writer, const Health& health);
[[nodiscard]] std::optional<Health> read_health(serialization::ByteReader& reader);

} // namespace atlas::health
