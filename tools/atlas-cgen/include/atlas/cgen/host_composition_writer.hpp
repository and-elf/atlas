#pragma once

#include "atlas/cgen/host_composition.hpp"

#include <string>
#include <string_view>

namespace atlas::cgen {

// Emits the generated header for a resolved HostComposition: a plain
// aggregate struct named after the host (spec §14; Rule of Zero - no
// invariant to protect, so no hand-written constructor, matching every
// other generated contract struct), one `runtime::PropertyStore<T>` member
// per property declared by every composed capability (in composition
// order, spec §5), and one `inline void register_property_stores(Context&,
// Host&)` free function that registers each of them - the mechanical
// PropertyStore-registration boilerplate spec §14 describes host
// composition as generating, replacing what a hand-written host
// (demo/tests/simulated_host.hpp before this) previously had to spell out
// itself, member by member.
//
// A composed capability with no properties (e.g. interruption, a pure
// event vocabulary) contributes no members and needs no #include.
// Requests and events are not this function's concern at all - only
// properties feed PropertyStore registration; a host's request-dispatch
// and event-subscription wiring is a separate, larger piece of
// manifest-driven composition not yet built (see the atlas-cgen README's
// own Scope section for why).
//
// #includes each composed capability's own *generated* contract header
// directly ("<capability_name>.capability.hpp",
// atlas_generate_capability_contract's own output naming) rather than a
// hand-written wrapper header - the generated contract is guaranteed to
// exist wherever contracts are generated at all, unlike a project's own
// wrapper-header convention (e.g. this demo's modules/<name>/<name>.hpp,
// spec §11), which atlas-cgen itself has no way to assume.
[[nodiscard]] std::string generate_host_composition(const HostComposition& composition,
                                                    std::string_view header_name,
                                                    std::string_view source_name);

} // namespace atlas::cgen
