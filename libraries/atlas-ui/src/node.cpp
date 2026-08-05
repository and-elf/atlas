#include "atlas/ui/node.hpp"

namespace atlas::ui {

std::optional<atlas::input::Intent> Node::try_click(atlas::Context& ctx, atlas::EntityRef source) const {
    if (!visible.resolve(ctx)) {
        return std::nullopt;
    }
    if (!clickable) {
        return std::nullopt;
    }
    return clickable->invoke(ctx, source);
}

} // namespace atlas::ui
