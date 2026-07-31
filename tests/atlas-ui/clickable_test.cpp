#include "atlas/entity/entity_ref.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/runtime/host.hpp"
#include "atlas/runtime/property_store.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"
#include "atlas/ui/clickable.hpp"

#include <gtest/gtest.h>

namespace atlas::ui {
namespace {

atlas::runtime::Host make_host() {
    auto sequence = atlas::stage::StageSequence::create({atlas::stage::StageId{"Simulation"}});
    return atlas::runtime::Host{std::move(*sequence), /*has_authority=*/true};
}

TEST(Clickable, InvokingAnEnabledBehaviorProducesAClickEventForTheSource) {
    auto host = make_host();
    atlas::Context ctx{host};
    const Clickable clickable{};

    const auto event = clickable.invoke(ctx, atlas::EntityRef{7, 0});

    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->source, (atlas::EntityRef{7, 0}));
}

TEST(Clickable, InvokingALiterallyDisabledBehaviorProducesNoEvent) {
    auto host = make_host();
    atlas::Context ctx{host};
    const Clickable clickable{.enabled = {.value = false}};

    const auto event = clickable.invoke(ctx, atlas::EntityRef{7, 0});

    EXPECT_FALSE(event.has_value());
}

// The enabled flag is itself a BindableProperty<bool> - a capability can
// disable a whole class of clickable nodes (e.g. "combat lockout") through
// ordinary property composition rather than every node's own literal flag.
TEST(Clickable, InvokingABehaviorDisabledThroughAComposedPropertyProducesNoEvent) {
    auto host = make_host();
    atlas::Context ctx{host};
    atlas::runtime::PropertyStore<bool> locked_out;
    locked_out.set(atlas::EntityRef{1, 0}, false);
    ctx.register_property_store(locked_out);
    const Clickable clickable{.enabled = {.value = true, .bound_entity = atlas::EntityRef{1, 0}}};

    const auto event = clickable.invoke(ctx, atlas::EntityRef{1, 0});

    EXPECT_FALSE(event.has_value());
}

} // namespace
} // namespace atlas::ui
