#include <gtest/gtest.h>
#include <sstream>
#include "experiment.hpp"
#include "ideal.hpp"


TEST(CacheHierarchyTest, CascadesEvictionsAcrossThreeLevels)
{
    caches::CacheLevels levels;
    levels.emplace_back(caches::Policy::LRU, 1);
    levels.emplace_back(caches::Policy::LRU, 1);
    levels.emplace_back(caches::Policy::LRU, 1);
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
    levels.emplace_back(caches::Policy::LRU, 1);
    levels.emplace_back(caches::Policy::LRU, 1);
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
    levels.emplace_back(caches::Policy::LRU, 1);
    levels.emplace_back(caches::Policy::LRU, 0);
    levels.emplace_back(caches::Policy::LRU, 1);
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

TEST (FullExperiment, CountsHitsAcrossTwoLevels)
{
    caches:: Config config;
    config.policies.resize(2);
    config.policies[0] = caches:: Policy:: LRU;
    config.policies[1] = caches:: Policy:: LRU;
    std:: istringstream input ("2 6 1 2 1 3 1 2"); 
    auto experiment_input = caches::ReadExperimentInput(input);
    size_t hits = caches::CountHierarchyHits(config, &experiment_input);
    EXPECT_EQ (hits, 2u);
}

TEST (FullExperiment, CountsHitsAcrossThreeLevels)
{
    caches:: Config config;
    config.policies.resize(3);
    config.policies[0] = caches:: Policy:: LRU;
    config.policies[1] = caches:: Policy:: LRU;
    config.policies[2] = caches:: Policy:: LRU;
    std:: istringstream input ("3 6 1 2 3 1 2 3"); 
    auto experiment_input = caches::ReadExperimentInput(input);
    size_t hits = caches::CountHierarchyHits(config, &experiment_input);
    EXPECT_EQ (hits, 3u);
}

TEST (FullExperiment, HandlesMoreLevelsThanCapacity)
{
    caches:: Config config;
    config.policies.resize(5);
    config.policies[0] = caches:: Policy:: LRU;
    config.policies[1] = caches:: Policy:: LRU;
    config.policies[2] = caches:: Policy:: LRU;
    config.policies[3] = caches:: Policy:: LRU;
    config.policies[4] = caches:: Policy:: LRU;
    std:: istringstream input ("2 6 1 2 1 2 1 2"); 
    auto experiment_input = caches::ReadExperimentInput(input);
    size_t hits = caches::CountHierarchyHits(config, &experiment_input);
    EXPECT_EQ (hits, 4u);
}

TEST (FullExperiment, ZeroCapacityHasNoHit)
{
    caches:: Config config;
    config.policies.resize(3);
    config.policies[0] = caches:: Policy:: LRU;
    config.policies[1] = caches:: Policy:: LRU;
    config.policies[2] = caches:: Policy:: LRU;
    std:: istringstream input ("0 3 1 1 1"); 
    auto experiment_input = caches::ReadExperimentInput(input);
    size_t hits = caches::CountHierarchyHits(config, &experiment_input);
    EXPECT_EQ (hits, 0u);
}

TEST (FullExperiment, NoRequestsHasNoHits)
{
    caches:: Config config;
    config.policies.resize(2);
    config.policies[0] = caches:: Policy:: LRU;
    config.policies[1] = caches:: Policy:: LRU;
    std:: istringstream input ("2 0"); 
    auto experiment_input = caches::ReadExperimentInput(input);
    size_t hits = caches::CountHierarchyHits(config, &experiment_input);
    EXPECT_EQ (hits, 0u);
}

TEST (FullExperiment, NoLevelsEmptyConfig)
{
    caches:: Config config;
    config.policies.resize(0);
    std:: istringstream input ("2 0");
    auto experiment_input = caches::ReadExperimentInput(input);
    EXPECT_THROW(caches::CountHierarchyHits(config, &experiment_input), std::invalid_argument);
}

TEST (FullExperiment, NotEnoughRequests)
{
    std:: istringstream input ("2 3 1");
    EXPECT_THROW(caches::ReadExperimentInput(input), std::runtime_error);
}

TEST (FullExperiment, MixedPolicy)
{
    caches:: Config config;
    config.policies.resize(2);
    config.policies[0] = caches:: Policy:: LFU;
    config.policies[1] = caches:: Policy:: LRU;
    std:: istringstream input ("3 7 1 1 2 3 2 4 1"); 
    auto experiment_input = caches::ReadExperimentInput(input);
    size_t hits = caches::CountHierarchyHits(config, &experiment_input);
    EXPECT_EQ (hits, 3u);
}

TEST(FullExperiment, SingleLevelLRUUsesRecency)
{
    caches::Config config;
    config.policies = {caches::Policy::LRU};
    std::istringstream input("2 6 1 1 2 3 1 2");

    auto experiment_input = caches::ReadExperimentInput(input);
    EXPECT_EQ(caches::CountHierarchyHits(config, &experiment_input), 1u);
}

TEST(FullExperiment, SingleLevelLFUUsesFrequency)
{
    caches::Config config;
    config.policies = {caches::Policy::LFU};
    std::istringstream input("2 6 1 1 2 3 1 2");

    auto experiment_input = caches::ReadExperimentInput(input);
    EXPECT_EQ(caches::CountHierarchyHits(config, &experiment_input), 2u);
}

TEST(FullExperiment, CompareIdealHitsAndHierarchyHits)
{
    caches::Config config;
    config.policies = {caches::Policy::LRU};
    caches::ExperimentInput experiment_input{2, {1, 2, 3, 1, 2, 3}};

    EXPECT_EQ(caches::CountHierarchyHits(config, &experiment_input), 0u);
    EXPECT_EQ(caches::CountIdealHits(&experiment_input), 2u);
}

TEST(FullExperiment, SingleLevelTwoQCountsGhostLookupAsMiss)
{
    caches::Config config;
    config.policies = {caches::Policy::TWO_Q};
    caches::ExperimentInput input{3, {1, 2, 3, 4, 1, 1, 5, 6, 1}};

    EXPECT_EQ(caches::CountHierarchyHits(config, &input), 2u);
    EXPECT_EQ(caches::CountIdealHits(&input), 3u);
}

TEST(FullExperiment, MixedTwoQHierarchyPromotesFromLowerLevel)
{
    caches::Config config;
    config.policies = {caches::Policy::TWO_Q, caches::Policy::LRU};
    caches::ExperimentInput input{3, {1, 2, 3, 1, 1, 4, 3}};

    EXPECT_EQ(caches::CountHierarchyHits(config, &input), 3u);
}

TEST(CacheHierarchyTest, TwoQPromotionCascadesWithoutDuplicatePages)
{
    caches::CacheLevels levels;
    levels.emplace_back(caches::Policy::TWO_Q, 2);
    levels.emplace_back(caches::Policy::LFU, 0);
    levels.emplace_back(caches::Policy::LRU, 1);
    caches::TransferContext context{levels, 1};

    for (int key : {1, 2, 3})
        levels[0].insert(key, caches::page_t{key * 100}, caches::CallBack, &context);
    EXPECT_FALSE(levels[0].touch(1));
    auto page = levels[2].extract(1);
    ASSERT_TRUE(page.has_value());
    EXPECT_EQ(page->id, 100);
    levels[0].insert(1, *page, caches::CallBack, &context);

    EXPECT_EQ(levels[0].size(), 2u);
    EXPECT_TRUE(levels[1].empty());
    EXPECT_EQ(levels[2].size(), 1u);
    EXPECT_FALSE(levels[0].extract(2).has_value());
    EXPECT_FALSE(levels[2].extract(1).has_value());
    EXPECT_FALSE(levels[2].extract(3).has_value());
    page = levels[0].extract(1);
    ASSERT_TRUE(page.has_value());
    EXPECT_EQ(page->id, 100);
    page = levels[0].extract(3);
    ASSERT_TRUE(page.has_value());
    EXPECT_EQ(page->id, 300);
    page = levels[2].extract(2);
    ASSERT_TRUE(page.has_value());
    EXPECT_EQ(page->id, 200);
    EXPECT_TRUE(levels[0].empty());
    EXPECT_TRUE(levels[2].empty());
}

TEST(FullExperiment, TwoQHierarchyHandlesMoreLevelsThanCapacity)
{
    caches::Config config;
    config.policies = {caches::Policy::TWO_Q, caches::Policy::LFU, caches::Policy::TWO_Q};
    caches::ExperimentInput input{1, {1, 1, 2, 1}};

    EXPECT_EQ(caches::CountHierarchyHits(config, &input), 1u);
}

TEST(FullExperiment, AllPoliciesHandleRequestsAndZeroCapacity)
{
    caches::Config config;
    config.policies = {caches::Policy::ARC, caches::Policy::LIRS, caches::Policy::TWO_Q,
                       caches::Policy::LFU, caches::Policy::LRU};
    caches::ExperimentInput input{3, {1, 2, 3, 1, 2, 3}};
    EXPECT_EQ(caches::CountHierarchyHits(config, &input), 3u);
    EXPECT_EQ(caches::CountHierarchyHits(config, &input), 3u);
    EXPECT_EQ(caches::CountIdealHits(&input), 3u);
    input.capacity = 0;
    EXPECT_EQ(caches::CountHierarchyHits(config, &input), 0u);
}

TEST(FullExperiment, RemainingSingleLevelPoliciesUseTheirOwnReplacementRules)
{
    caches::ExperimentInput arc_input{2, {1, 2, 1, 3, 2, 1, 4, 3, 2, 5}};
    caches::Config arc_config;
    arc_config.policies = {caches::Policy::ARC};
    EXPECT_EQ(caches::CountHierarchyHits(arc_config, &arc_input), 1u);

    caches::ExperimentInput lirs_input{3, {1, 2, 3, 4, 3, 5, 5, 2, 6}};
    caches::Config lirs_config;
    lirs_config.policies = {caches::Policy::LIRS};
    EXPECT_EQ(caches::CountHierarchyHits(lirs_config, &lirs_input), 2u);
}

TEST(CacheHierarchyTest, RemainingPoliciesPromoteThroughZeroCapacityWithoutDuplicates)
{
    for (auto policy : {caches::Policy::ARC, caches::Policy::LIRS})
    {
        caches::CacheLevels levels;
        levels.emplace_back(policy, 2);
        levels.emplace_back(caches::Policy::TWO_Q, 0);
        levels.emplace_back(caches::Policy::LRU, 1);
        caches::TransferContext context{levels, 1};
        for (int key : {1, 2, 3})
            levels[0].insert(key, caches::page_t{key * 100}, caches::CallBack, &context);

        const int promoted_key = policy == caches::Policy::ARC ? 1 : 2;
        auto page = levels[2].extract(promoted_key);
        ASSERT_TRUE(page.has_value());
        EXPECT_EQ(page->id, promoted_key * 100);
        levels[0].insert(promoted_key, *page, caches::CallBack, &context);

        const int lower_key = policy == caches::Policy::ARC ? 2 : 3;
        EXPECT_EQ(levels[0].size(), 2u);
        EXPECT_TRUE(levels[1].empty());
        EXPECT_EQ(levels[2].size(), 1u);
        for (int key : {1, 2, 3})
        {
            auto upper = levels[0].extract(key);
            auto lower = levels[2].extract(key);
            EXPECT_EQ(upper.has_value(), key != lower_key);
            EXPECT_EQ(lower.has_value(), key == lower_key);
            if (upper)
            {
                EXPECT_EQ(upper->id, key * 100);
            }
            if (lower)
            {
                EXPECT_EQ(lower->id, key * 100);
            }
        }
        EXPECT_TRUE(levels[0].empty());
        EXPECT_TRUE(levels[2].empty());
    }
}

