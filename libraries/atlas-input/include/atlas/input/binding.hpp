#pragma once

#include "atlas/input/intent.hpp"
#include "atlas/input/raw_signal.hpp"

namespace atlas::input {

// One entry in a binding configuration: which raw signal produces which
// intent (spec §5, Input as Intent - "Binding configuration is data, not
// code... authored as a binding config file, player-editable at runtime, no
// recompile, live rebind"). InputBinding itself is exactly that data - one
// production rule, never a callback or branch - so a real binding config is
// simply a `std::vector<InputBinding>` that could equally be loaded from a
// file at runtime with no change to IntentRouter; this pass builds that
// vector in-memory (see README) since a config-file format is a separate,
// not-yet-scoped concern.
//
// A basic aggregate (rule of zero): identity is just the pair of names.
struct InputBinding {
    RawSignalId raw_signal;
    IntentId intent;

    friend constexpr bool operator==(const InputBinding&, const InputBinding&) noexcept = default;
};

} // namespace atlas::input
