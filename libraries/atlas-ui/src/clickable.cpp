#include "atlas/ui/clickable.hpp"

namespace atlas::ui {

std::optional<atlas::input::Intent> Clickable::invoke(atlas::Context& ctx, atlas::EntityRef source) const {
    if (!enabled.resolve(ctx)) {
        return std::nullopt;
    }
    return atlas::input::Intent{.id = intent, .entity = source};
}

} // namespace atlas::ui
