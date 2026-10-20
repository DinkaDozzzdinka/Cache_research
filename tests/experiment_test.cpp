#include "experiment.hpp"
#include <gtest/gtest.h>

TEST(CacheHierarchyTest, CascadesEvictionsAcrossThreeLevels)
{
    caches::CacheLevels levels;
    levels.emplace_back(1);
    levels.emplace_back(1);
    levels.emplace_back(1);
    caches::TransferContext context {levels, 1};

    levels[0].insert(1, caches::page_t{1}, caches::CallBack, &context);
    levels[0].insert(2, caches::page_t{2}, caches::CallBack, &context);
    levels[0].insert(3, caches::page_t{3}, caches::CallBack, &context);
    levels[0].insert(4, caches::page_t{4}, caches::CallBack, &context);

    for (auto& level : levels)
    {
        EXPECT_EQ(level.size(), 1u);
        EXPECT_FALSE(level.extract(1).has_value());
    }
    EXPECT_FALSE(levels[0].extract(2).has_value());
    EXPECT_FALSE(levels[0].extract(3).has_value());
    EXPECT_FALSE(levels[1].extract(2).has_value());
    EXPECT_FALSE(levels[1].extract(4).has_value());
    EXPECT_FALSE(levels[2].extract(3).has_value());
    EXPECT_FALSE(levels[2].extract(4).has_value());

    auto page = levels[0].extract(4);
    ASSERT_TRUE(page.has_value());
    EXPECT_EQ(page->id, 4);
    page = levels[1].extract(3);
    ASSERT_TRUE(page.has_value());
    EXPECT_EQ(page->id, 3);
    page = levels[2].extract(2);
    ASSERT_TRUE(page.has_value());
    EXPECT_EQ(page->id, 2);

    for (auto& level : levels)
        EXPECT_TRUE(level.empty());
}

TEST(CacheHierarchyTest, PromotesPageWithoutDuplicates)
{
    caches::CacheLevels levels;
    levels.emplace_back(1);
    levels.emplace_back(1);
    caches::TransferContext context{levels, 1};

    levels[0].insert(1, caches::page_t{1}, caches::CallBack, &context);
    levels[0].insert(2, caches::page_t{2}, caches::CallBack, &context);
    EXPECT_EQ(levels[0].size(), 1u);
    EXPECT_EQ(levels[1].size(), 1u);

    auto promoted = levels[1].extract(1);
    ASSERT_TRUE(promoted.has_value());
    EXPECT_EQ(promoted->id, 1);
    EXPECT_TRUE(levels[1].empty());
    levels[0].insert(1, *promoted, caches::CallBack, &context);

    EXPECT_EQ(levels[0].size(), 1u);
    EXPECT_EQ(levels[1].size(), 1u);
    EXPECT_FALSE(levels[0].extract(2).has_value());
    EXPECT_FALSE(levels[1].extract(1).has_value());

    auto page = levels[0].extract(1);
    ASSERT_TRUE(page.has_value());
    EXPECT_EQ(page->id, 1);
    page = levels[1].extract(2);
    ASSERT_TRUE(page.has_value());
    EXPECT_EQ(page->id, 2);
    EXPECT_TRUE(levels[0].empty());
    EXPECT_TRUE(levels[1].empty());
}

TEST(CacheHierarchyTest, ForwardsThroughZeroCapacityLevel)
{
    caches::CacheLevels levels;
    levels.emplace_back(1);
    levels.emplace_back(0);
    levels.emplace_back(1);
    caches::TransferContext context{levels, 1};

    levels[0].insert(1, caches::page_t{1}, caches::CallBack, &context);
    levels[0].insert(2, caches::page_t{2}, caches::CallBack, &context);
    levels[0].insert(3, caches::page_t{3}, caches::CallBack, &context);
    EXPECT_EQ(levels[0].size(), 1u);
    EXPECT_EQ(levels[1].size(), 0u);
    EXPECT_EQ(levels[2].size(), 1u);
    EXPECT_TRUE(levels[1].empty());

    for (auto& level : levels)
        EXPECT_FALSE(level.extract(1).has_value());
    EXPECT_FALSE(levels[0].extract(2).has_value());
    EXPECT_FALSE(levels[2].extract(3).has_value());
    for (int key = 2; key <= 3; ++key)
        EXPECT_FALSE(levels[1].extract(key).has_value());

    auto page = levels[0].extract(3);
    ASSERT_TRUE(page.has_value());
    EXPECT_EQ(page->id, 3);
    page = levels[2].extract(2);
    ASSERT_TRUE(page.has_value());
    EXPECT_EQ(page->id, 2);

    for (auto& level : levels)
        EXPECT_TRUE(level.empty());
}
