#include "armor.hpp"

#include <stdexcept>

namespace atlas::armor {

void add_contribution(Context& ctx,
                      ContributionRegistry& registry,
                      EntityRef entity,
                      std::string_view source,
                      std::int32_t value) {
    auto armor = ctx.get<Armor>(entity);
    if (!armor) {
        throw std::logic_error("atlas::armor::add_contribution: entity has no Armor property");
    }

    registry[entity].push_back(runtime::Contribution<std::int32_t>{.source = source, .value = value});

    // Armor's manifest declares no explicit base value literal (atlas-cgen
    // does not parse per-field default-value literals yet, only type
    // tokens - see tools/atlas-cgen/README.md), so its generated struct's
    // base defaults to 0 via aggregate zero-initialization; 0 here matches
    // that declared starting point exactly, not an arbitrary choice.
    armor->get().base = runtime::resolve_additive<std::int32_t>(0, registry[entity]);
}

} // namespace atlas::armor
