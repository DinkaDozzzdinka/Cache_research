#include "workloads.hpp"
#include <gtest/gtest.h>
#include <limits>
#include <set>

TEST(WorkloadsTest, SequentialKeysHaveNoRepeats)
{
    const std::vector<int> expected{10, 11, 12, 13};
    EXPECT_EQ(caches::GenerateSequential(4, 10), expected);
    EXPECT_TRUE(caches::GenerateSequential(0).empty());
    EXPECT_THROW(caches::GenerateSequential(2, std::numeric_limits<int>::max()), std::invalid_argument);
}

TEST(WorkloadsTest, CycleHasExactLengthAndWorkingSet)
{
    const std::vector<int> expected{0, 1, 2, 0, 1, 2, 0, 1};
    EXPECT_EQ(caches::GenerateCycle(8, 3), expected);
    EXPECT_TRUE(caches::GenerateCycle(0, 3).empty());
    EXPECT_THROW(caches::GenerateCycle(5, 0), std::invalid_argument);
    EXPECT_THROW(caches::GenerateCycle(5, -1), std::invalid_argument);
    EXPECT_THROW(caches::GenerateCycle(5, 2, std::numeric_limits<int>::max()), std::invalid_argument);
}

TEST(WorkloadsTest, UniformIsRepeatableAndRespectsRange)
{
    const auto first = caches::GenerateUniform(1000, 7, 42);
    EXPECT_EQ(first, caches::GenerateUniform(1000, 7, 42));
    EXPECT_NE(first, caches::GenerateUniform(1000, 7, 43));
    ASSERT_EQ(first.size(), 1000u);
    for (int key : first)
        EXPECT_TRUE(key >= 0 && key < 7);
    EXPECT_THROW(caches::GenerateUniform(5, 0, 42), std::invalid_argument);
}

TEST(WorkloadsTest, PhaseChangeUsesDisjointWorkingSets)
{
    const std::vector<int> expected{0, 1, 0, 1, 2, 3, 2, 3};
    EXPECT_EQ(caches::GeneratePhaseChange(8, 2), expected);
    EXPECT_EQ(caches::GeneratePhaseChange(1, 2), std::vector<int>{2});
    EXPECT_TRUE(caches::GeneratePhaseChange(0, 2).empty());
    EXPECT_THROW(caches::GeneratePhaseChange(5, 0), std::invalid_argument);
}

TEST(WorkloadsTest, HotScanSeparatesHotAndUniqueColdKeys)
{
    const std::vector<int> expected{0, 1, 0, 1, 2, 3, 0, 1, 0, 1, 4, 5, 0};
    EXPECT_EQ(caches::GenerateHotScan(13, 2, 4, 2), expected);
    EXPECT_TRUE(caches::GenerateHotScan(0, 2, 4, 2).empty());
    EXPECT_THROW(caches::GenerateHotScan(10, 0, 4, 2), std::invalid_argument);
    EXPECT_THROW(caches::GenerateHotScan(10, 2, 0, 2), std::invalid_argument);
    EXPECT_THROW(caches::GenerateHotScan(10, 2, 4, 0), std::invalid_argument);
}
