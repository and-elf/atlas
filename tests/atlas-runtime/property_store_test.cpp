#include "atlas/runtime/property_store.hpp"

#include <cstdint>
#include <gtest/gtest.h>

namespace atlas::runtime {
namespace {

TEST(PropertyStore, GetOnAnEntityWithNoStoredValueReturnsNullopt) {
    PropertyStore<std::int32_t> store;

    EXPECT_FALSE(store.get(EntityRef{1, 0}).has_value());
}

TEST(PropertyStore, SetThenGetReturnsTheStoredValue) {
    PropertyStore<std::int32_t> store;

    store.set(EntityRef{1, 0}, 42);

    const auto value = store.get(EntityRef{1, 0});
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get(), 42);
}

TEST(PropertyStore, SetOverwritesAPreviouslyStoredValue) {
    PropertyStore<std::int32_t> store;

    store.set(EntityRef{1, 0}, 42);
    store.set(EntityRef{1, 0}, 7);

    const auto value = store.get(EntityRef{1, 0});
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get(), 7);
}

TEST(PropertyStore, DifferentEntitiesAreStoredIndependently) {
    PropertyStore<std::int32_t> store;

    store.set(EntityRef{1, 0}, 10);
    store.set(EntityRef{2, 0}, 20);

    EXPECT_EQ(store.get(EntityRef{1, 0})->get(), 10);
    EXPECT_EQ(store.get(EntityRef{2, 0})->get(), 20);
}

TEST(PropertyStore, GetReturnsAMutableReferenceThatWritesThrough) {
    PropertyStore<std::int32_t> store;
    store.set(EntityRef{1, 0}, 10);

    auto value = store.get(EntityRef{1, 0});
    ASSERT_TRUE(value.has_value());
    value->get() = 99;

    EXPECT_EQ(store.get(EntityRef{1, 0})->get(), 99);
}

TEST(PropertyStore, ConstStoreReturnsAConstReference) {
    PropertyStore<std::int32_t> store;
    store.set(EntityRef{1, 0}, 10);

    const PropertyStore<std::int32_t>& const_store = store;
    const auto value = const_store.get(EntityRef{1, 0});
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get(), 10);
}

TEST(PropertyStore, ConstStoreGetOnAnEntityWithNoStoredValueReturnsNullopt) {
    const PropertyStore<std::int32_t> store;

    EXPECT_FALSE(store.get(EntityRef{1, 0}).has_value());
}

TEST(PropertyStore, ResetClearsAllStoredValues) {
    PropertyStore<std::int32_t> store;
    store.set(EntityRef{1, 0}, 10);
    store.set(EntityRef{2, 0}, 20);

    store.reset();

    EXPECT_FALSE(store.get(EntityRef{1, 0}).has_value());
    EXPECT_FALSE(store.get(EntityRef{2, 0}).has_value());
}

TEST(PropertyStore, ResetOnAnEmptyStoreIsHarmless) {
    PropertyStore<std::int32_t> store;

    EXPECT_NO_THROW(store.reset());
    EXPECT_FALSE(store.get(EntityRef{1, 0}).has_value());
}

TEST(PropertyStore, SetAfterResetIsObservedAgain) {
    PropertyStore<std::int32_t> store;
    store.set(EntityRef{1, 0}, 10);
    store.reset();

    store.set(EntityRef{1, 0}, 99);

    const auto value = store.get(EntityRef{1, 0});
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get(), 99);
}

} // namespace
} // namespace atlas::runtime
