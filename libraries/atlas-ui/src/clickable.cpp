#include "atlas/ui/clickable.hpp"

namespace atlas::ui {

std::optional<ClickEvent> Clickable::invoke(atlas::Context& ctx, atlas::EntityRef source) const {
    if (!enabled.resolve(ctx)) {
        return std::nullopt;
    }
    return ClickEvent{.source = source};
}

} // namespace atlas::ui
