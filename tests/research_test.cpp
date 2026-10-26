#include "research.hpp"
#include "experiment.hpp"
#include <gtest/gtest.h>
#include <set>

TEST(ResearchTest, EnumeratesAllOrderedConfigurations)
{
    const auto configurations = caches::EnumerateConfigurations(3);
    ASSERT_EQ(configurations.size(), 155u);
    std::set<std::string> names;
    std::size_t counts[4]{};
    for (const auto& config : configurations)
    {
        ASSERT_TRUE(config.policies.size() >= 1 && config.policies.size() <= 3);
        ++counts[config.policies.size()];
        names.insert(caches::ConfigurationName(config));
    }
    EXPECT_EQ(names.size(), 155u);
    EXPECT_EQ(counts[1], 5u);
    EXPECT_EQ(counts[2], 25u);
    EXPECT_EQ(counts[3], 125u);
    EXPECT_EQ(names.count("ARC|LRU"), 1u);
    EXPECT_EQ(names.count("LRU|ARC"), 1u);
    EXPECT_EQ(caches::EnumerateConfigurations(1).size(), 5u);
    EXPECT_THROW(caches::EnumerateConfigurations(0), std::invalid_argument);
    EXPECT_THROW(caches::EnumerateConfigurations(4), std::invalid_argument);
}

TEST(ResearchTest, ReportsActualLevelCapacities)
{
    EXPECT_EQ(caches::LevelCapacities(8, 3), "3|3|2");
    EXPECT_EQ(caches::LevelCapacities(2, 3), "1|1|0");
    EXPECT_EQ(caches::LevelCapacities(0, 3), "0|0|0");
    EXPECT_THROW(caches::LevelCapacities(8, 0), std::invalid_argument);
}

TEST(ResearchTest, PolicyNamesMatchConfigParser)
{
    for (auto policy : {caches::Policy::LRU, caches::Policy::LFU, caches::Policy::TWO_Q,
                        caches::Policy::ARC, caches::Policy::LIRS})
        EXPECT_EQ(caches::ParsePolicy(caches::PolicyName(policy)), policy);
    EXPECT_THROW(caches::PolicyName(static_cast<caches::Policy>(999)), std::invalid_argument);
}

TEST(ResearchTest, EveryCandidateCanRunIndependently)
{
    caches::ExperimentInput input{3, {1, 2, 3, 1, 2, 3, 4, 1, 4}};
    const auto ideal = caches::CountIdealHits(&input);
    for (const auto& config : caches::EnumerateConfigurations(3))
    {
        const auto hits = caches::CountHierarchyHits(config, &input);
        EXPECT_LE(hits, ideal);
        EXPECT_EQ(caches::CountHierarchyHits(config, &input), hits);
    }
}
