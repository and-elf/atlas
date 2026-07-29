#include "atlas/stage/stage_id.hpp"
#include "atlas/stage/stage_sequence.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace atlas::stage {
namespace {

TEST(StageId, EqualityComparesByName) {
    EXPECT_EQ((StageId{"Simulation"}), (StageId{"Simulation"}));
    EXPECT_NE((StageId{"Simulation"}), (StageId{"Presentation"}));
}

TEST(StageSequence, EmptySequenceIsValidAndIterates0Times) {
    const auto sequence = StageSequence::create({});

    ASSERT_TRUE(sequence.has_value());
    EXPECT_EQ(sequence->size(), 0U);
    EXPECT_EQ(sequence->begin(), sequence->end());
}

TEST(StageSequence, PreservesInsertionOrder) {
    const std::vector<StageId> stages{StageId{"Input"}, StageId{"Simulation"}, StageId{"Presentation"}};

    const auto sequence = StageSequence::create(stages);

    ASSERT_TRUE(sequence.has_value());
    ASSERT_EQ(sequence->size(), 3U);
    EXPECT_EQ(std::vector(sequence->begin(), sequence->end()), stages);
}

TEST(StageSequence, RejectsDuplicateStage) {
    const std::vector<StageId> stages{StageId{"Simulation"}, StageId{"Presentation"}, StageId{"Simulation"}};

    const auto sequence = StageSequence::create(stages);

    EXPECT_FALSE(sequence.has_value());
}

TEST(StageSequence, SingleStageSequenceHasNoDuplicate) {
    const auto sequence = StageSequence::create({StageId{"Simulation"}});

    ASSERT_TRUE(sequence.has_value());
    EXPECT_EQ(sequence->size(), 1U);
}

TEST(StageSequence, ContainsReportsMembership) {
    const auto sequence = StageSequence::create({StageId{"Input"}, StageId{"Simulation"}});

    ASSERT_TRUE(sequence.has_value());
    EXPECT_TRUE(sequence->contains(StageId{"Input"}));
    EXPECT_FALSE(sequence->contains(StageId{"Replication"}));
}

TEST(StageSequence, IterationOrderIsBitExactAcrossRepeatedTraversals) {
    const auto sequence = StageSequence::create(
        {StageId{"Input"}, StageId{"Simulation"}, StageId{"Replication"}, StageId{"Presentation"}});
    ASSERT_TRUE(sequence.has_value());

    // Determinism guarantee (spec 4): the same fixed sequence must produce the
    // identical ordering on every traversal, not merely an ordering with the
    // same elements.
    const std::vector<StageId> first_pass(sequence->begin(), sequence->end());
    const std::vector<StageId> second_pass(sequence->begin(), sequence->end());
    const std::vector<StageId> third_pass(sequence->begin(), sequence->end());

    EXPECT_EQ(first_pass, second_pass);
    EXPECT_EQ(second_pass, third_pass);
}

} // namespace
} // namespace atlas::stage
