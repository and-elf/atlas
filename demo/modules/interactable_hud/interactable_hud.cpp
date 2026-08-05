#include "interactable_hud.hpp"

namespace atlas::interactable_hud {

std::optional<atlas::ui::Node> build_control(atlas::Context& ctx, atlas::EntityRef entity) {
    const auto interactable = ctx.get<atlas::interactable::Interactable>(entity);
    if (!interactable) {
        return std::nullopt;
    }
    return atlas::ui::Node{
        .resource = {.value = interactable->get().designator},
        .clickable = atlas::ui::Clickable{.intent = interactable->get().action},
    };
}

} // namespace atlas::interactable_hud
