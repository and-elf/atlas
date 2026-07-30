#include "atlas/core/time.hpp"
#include "atlas/entity/entity_ref.hpp"
#include "atlas/render/frame.hpp"
#include "atlas/render/frame_builder.hpp"
#include "atlas/render/renderable.hpp"
#include "atlas/render/transform.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/runtime/property_store.hpp"

#include <array>
#include <gtest/gtest.h>

namespace atlas::render {
namespace {

TEST(BuildFrame, EmptySceneProducesAnEmptyFrame) {
    const runtime::PropertyStore<Transform> transforms;
    const runtime::PropertyStore<Renderable> renderables;

    const Frame frame = build_frame({}, transforms, renderables, core::Time{.ticks = 42});

    EXPECT_TRUE(frame.draw_commands.empty());
    EXPECT_EQ(frame.tick.ticks, 42U);
}

TEST(BuildFrame, EntityWithoutATransformIsSkipped) {
    runtime::PropertyStore<Transform> transforms;
    runtime::PropertyStore<Renderable> renderables;
    const EntityRef entity{1, 0};
    renderables.set(
        entity,
        Renderable{.mesh = ResourceId::from_name("mesh"), .material = ResourceId::from_name("material")});

    const std::array<EntityRef, 1> entities{entity};
    const Frame frame = build_frame(entities, transforms, renderables, core::Time{});

    EXPECT_TRUE(frame.draw_commands.empty());
}

TEST(BuildFrame, EntityWithoutARenderableIsSkipped) {
    runtime::PropertyStore<Transform> transforms;
    runtime::PropertyStore<Renderable> renderables;
    const EntityRef entity{1, 0};
    transforms.set(entity, Transform{});

    const std::array<EntityRef, 1> entities{entity};
    const Frame frame = build_frame(entities, transforms, renderables, core::Time{});

    EXPECT_TRUE(frame.draw_commands.empty());
}

TEST(BuildFrame, EntityWithANullMeshReferenceIsSkipped) {
    runtime::PropertyStore<Transform> transforms;
    runtime::PropertyStore<Renderable> renderables;
    const EntityRef entity{1, 0};
    transforms.set(entity, Transform{});
    renderables.set(entity, Renderable{.mesh = ResourceId{}, .material = ResourceId::from_name("material")});

    const std::array<EntityRef, 1> entities{entity};
    const Frame frame = build_frame(entities, transforms, renderables, core::Time{});

    EXPECT_TRUE(frame.draw_commands.empty());
}

TEST(BuildFrame, EntityWithANullMaterialReferenceIsSkipped) {
    runtime::PropertyStore<Transform> transforms;
    runtime::PropertyStore<Renderable> renderables;
    const EntityRef entity{1, 0};
    transforms.set(entity, Transform{});
    renderables.set(entity, Renderable{.mesh = ResourceId::from_name("mesh"), .material = ResourceId{}});

    const std::array<EntityRef, 1> entities{entity};
    const Frame frame = build_frame(entities, transforms, renderables, core::Time{});

    EXPECT_TRUE(frame.draw_commands.empty());
}

TEST(BuildFrame, FullyResolvedEntityProducesOneDrawCommand) {
    runtime::PropertyStore<Transform> transforms;
    runtime::PropertyStore<Renderable> renderables;
    const EntityRef entity{1, 0};
    const Transform transform{.position = {.x = 1.0F, .y = 2.0F, .z = 3.0F},
                              .rotation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F}};
    const auto mesh = ResourceId::from_name("characters/hero/mesh");
    const auto material = ResourceId::from_name("characters/hero/material");
    transforms.set(entity, transform);
    renderables.set(entity, Renderable{.mesh = mesh, .material = material});

    const std::array<EntityRef, 1> entities{entity};
    const Frame frame = build_frame(entities, transforms, renderables, core::Time{.ticks = 7});

    ASSERT_EQ(frame.draw_commands.size(), 1U);
    const DrawCommand& draw = frame.draw_commands.front();
    EXPECT_EQ(draw.entity, entity);
    EXPECT_FLOAT_EQ(draw.transform.position.x, 1.0F);
    EXPECT_EQ(draw.mesh, mesh);
    EXPECT_EQ(draw.material, material);
    EXPECT_EQ(frame.tick.ticks, 7U);
}

TEST(BuildFrame, MultipleEntitiesAreVisitedInTheCallerSuppliedOrder) {
    runtime::PropertyStore<Transform> transforms;
    runtime::PropertyStore<Renderable> renderables;
    const EntityRef first{1, 0};
    const EntityRef second{2, 0};
    const auto mesh = ResourceId::from_name("mesh");
    const auto material = ResourceId::from_name("material");

    for (const auto entity : {first, second}) {
        transforms.set(entity, Transform{});
        renderables.set(entity, Renderable{.mesh = mesh, .material = material});
    }

    // Deliberately supplied in reverse-of-creation order - build_frame must
    // follow this order exactly, never re-derive its own via, say, an
    // unordered_map's own iteration order (spec §4, determinism).
    const std::array<EntityRef, 2> entities{second, first};
    const Frame frame = build_frame(entities, transforms, renderables, core::Time{});

    ASSERT_EQ(frame.draw_commands.size(), 2U);
    EXPECT_EQ(frame.draw_commands.at(0).entity, second);
    EXPECT_EQ(frame.draw_commands.at(1).entity, first);
}

TEST(BuildFrame, RepeatedCallsWithIdenticalInputProduceIdenticalOutput) {
    runtime::PropertyStore<Transform> transforms;
    runtime::PropertyStore<Renderable> renderables;
    const EntityRef entity{3, 0};
    const Transform transform{.position = {.x = 4.0F, .y = 5.0F, .z = 6.0F},
                              .rotation = {.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F}};
    const auto mesh = ResourceId::from_name("mesh");
    const auto material = ResourceId::from_name("material");
    transforms.set(entity, transform);
    renderables.set(entity, Renderable{.mesh = mesh, .material = material});

    const std::array<EntityRef, 1> entities{entity};

    const Frame first_frame = build_frame(entities, transforms, renderables, core::Time{.ticks = 1});
    const Frame second_frame = build_frame(entities, transforms, renderables, core::Time{.ticks = 1});

    ASSERT_EQ(first_frame.draw_commands.size(), second_frame.draw_commands.size());
    EXPECT_EQ(first_frame.draw_commands.front().entity, second_frame.draw_commands.front().entity);
    EXPECT_EQ(first_frame.draw_commands.front().mesh, second_frame.draw_commands.front().mesh);
    EXPECT_FLOAT_EQ(first_frame.draw_commands.front().transform.position.x,
                    second_frame.draw_commands.front().transform.position.x);
}

} // namespace
} // namespace atlas::render
