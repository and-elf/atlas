#include "atlas/entity/entity_ref.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/runtime/host.hpp"
#include "atlas/runtime/property_store.hpp"
#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"
#include "atlas/ui/bindable_property.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace atlas::ui {
namespace {

// A tiny stand-in composed game property, local to this test - the UI layer
// never mentions a real capability's property by name (spec §2, Mechanism
// Over Meaning); it only ever binds against whatever contract type a real
// capability declares, and Score plays that role here.
struct Score {
    float value;
};

atlas::runtime::Host make_host() {
    auto sequence = atlas::stage::StageSequence::create({atlas::stage::StageId{"Simulation"}});
    return atlas::runtime::Host{std::move(*sequence), /*has_authority=*/true};
}

TEST(BindableProperty, UnboundResolvesToItsLiteralValue) {
    auto host = make_host();
    atlas::Context ctx{host};
    const BindableProperty<float> property{.value = 3.5F};

    EXPECT_FALSE(property.is_bound());
    EXPECT_FLOAT_EQ(property.resolve(ctx), 3.5F);
}

TEST(BindableProperty, BoundToARegisteredPropertyResolvesTheComposedValue) {
    auto host = make_host();
    atlas::Context ctx{host};
    atlas::runtime::PropertyStore<Score> scores;
    scores.set(atlas::EntityRef{1, 0}, Score{.value = 42.0F});
    ctx.register_property_store(scores);

    const BindableProperty<Score> property{.value = Score{.value = 0.0F},
                                           .bound_entity = atlas::EntityRef{1, 0}};

    EXPECT_TRUE(property.is_bound());
    EXPECT_FLOAT_EQ(property.resolve(ctx).value, 42.0F);
}

// Spec §20, Continuous Re-resolution: a bound property re-reads the
// composed value on every resolution rather than caching the value seen at
// bind time - this is what makes `resolve()` correct to call every tick.
TEST(BindableProperty, ReResolvesAgainstTheStoresCurrentValueEveryCall) {
    auto host = make_host();
    atlas::Context ctx{host};
    atlas::runtime::PropertyStore<Score> scores;
    scores.set(atlas::EntityRef{1, 0}, Score{.value = 1.0F});
    ctx.register_property_store(scores);
    const BindableProperty<Score> property{.value = Score{.value = 0.0F},
                                           .bound_entity = atlas::EntityRef{1, 0}};

    EXPECT_FLOAT_EQ(property.resolve(ctx).value, 1.0F);
    scores.set(atlas::EntityRef{1, 0}, Score{.value = 2.0F});
    EXPECT_FLOAT_EQ(property.resolve(ctx).value, 2.0F);
}

// A binding to a property that doesn't resolve - the store is registered,
// but this particular entity has never had a value set for it - falls back
// to the literal default rather than throwing, matching Context::get<T>()'s
// own "no stored value is an ordinary, expected outcome" contract.
TEST(BindableProperty, BoundButUnresolvedFallsBackToTheLiteralDefault) {
    auto host = make_host();
    atlas::Context ctx{host};
    atlas::runtime::PropertyStore<Score> scores;
    ctx.register_property_store(scores);
    const BindableProperty<Score> property{.value = Score{.value = -1.0F},
                                           .bound_entity = atlas::EntityRef{1, 0}};

    EXPECT_FLOAT_EQ(property.resolve(ctx).value, -1.0F);
}

// A binding to a property whose store was never registered at all is a
// setup mistake, not an ordinary absence - it propagates Context::get<T>()'s
// own std::logic_error rather than silently falling back.
TEST(BindableProperty, BoundWithNoStoreRegisteredPropagatesTheSetupError) {
    auto host = make_host();
    atlas::Context ctx{host};
    const BindableProperty<Score> property{.value = Score{.value = 0.0F},
                                           .bound_entity = atlas::EntityRef{1, 0}};

    EXPECT_THROW((void)property.resolve(ctx), std::logic_error);
}

TEST(BindableProperty, ResourceIdPropertiesBindTheSameWayAnyOtherPropertyDoes) {
    auto host = make_host();
    atlas::Context ctx{host};
    atlas::runtime::PropertyStore<atlas::ResourceId> icons;
    const auto fireball_icon = atlas::ResourceId::from_name("icons/fireball");
    icons.set(atlas::EntityRef{2, 0}, fireball_icon);
    ctx.register_property_store(icons);

    const BindableProperty<atlas::ResourceId> icon{.bound_entity = atlas::EntityRef{2, 0}};

    EXPECT_EQ(icon.resolve(ctx), fireball_icon);
}

} // namespace
} // namespace atlas::ui
