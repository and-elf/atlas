#pragma once

#include "atlas/input/binding.hpp"
#include "atlas/input/intent.hpp"
#include "atlas/input/raw_signal.hpp"

#include <vector>

namespace atlas::input {

// The mechanism itself (spec §5, Input as Intent): given a raw-signal stream
// polled from an injectable RawSignalSource and a fixed set of bindings,
// produces the Intent events a capability may observe. This is the only
// place in the library where a RawSignalId is ever compared against
// binding configuration - poll()'s return type is `std::vector<Intent>`,
// so nothing raw ever escapes past this call.
//
// An encapsulated class rather than a plain aggregate: bindings_ is
// constructor-supplied and never mutated afterward, matching how a real
// binding configuration is fixed for the lifetime of a host session (loaded
// once, live-rebound by replacing the whole router - not mutated field by
// field mid-session); nothing here needs a setter.
class IntentRouter {
public:
    explicit IntentRouter(std::vector<InputBinding> bindings) : bindings_(std::move(bindings)) {}

    // Advances one poll of `source`, resolving each raw signal observed this
    // poll against this router's bindings, in the order they were observed.
    // An unbound raw signal (no binding names it) is silently ignored -
    // exactly like §5's own framing that a capability never sees "was E
    // pressed": a signal nothing binds to is simply not part of the game's
    // intent vocabulary right now, not an error.
    //
    // Templated on the RawSignalSource concept (§5, Tiny Interface
    // Composability's structural-typing philosophy applied to this seam)
    // rather than a virtual interface, so a future OS backend or this
    // library's own ScriptedRawSignalSource plug in identically with zero
    // runtime dispatch cost.
    template <RawSignalSource Source> [[nodiscard]] std::vector<Intent> poll(Source& source) const {
        std::vector<Intent> intents;
        for (const RawSignalEvent& event : source.poll()) {
            if (const IntentId* intent_id = find_binding(event.signal); intent_id != nullptr) {
                intents.push_back(Intent{.id = *intent_id, .axis = event.value});
            }
        }
        return intents;
    }

private:
    [[nodiscard]] const IntentId* find_binding(const RawSignalId& signal) const noexcept;

    std::vector<InputBinding> bindings_;
};

} // namespace atlas::input
