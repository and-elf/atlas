#include "atlas/input/intent_router.hpp"

namespace atlas::input {

const IntentId* IntentRouter::find_binding(const RawSignalId& signal) const noexcept {
    for (const InputBinding& binding : bindings_) {
        if (binding.raw_signal == signal) {
            return &binding.intent;
        }
    }
    return nullptr;
}

} // namespace atlas::input
