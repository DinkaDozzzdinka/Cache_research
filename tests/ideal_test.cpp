#include "ideal.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>

TEST(IdealCacheTest, EmptyRequestsHaveNoHits)
{
    const std::vector<int> requests;

    EXPECT_EQ(caches::CountIdealHits(2, requests), 0u);
}

TEST(IdealCacheTest, ZeroCapacityHasNoHits)
{
    const std::vector<int> requests = {1, 1, 2, 1};

    EXPECT_EQ(caches::CountIdealHits(0, requests), 0u);
}

TEST(IdealCacheTest, KeepsAllPagesWhenTheyFit)
{
    const std::vector<int> requests = {1, 2, 1, 3, 2, 1};

    EXPECT_EQ(caches::CountIdealHits(3, requests), 3u);
}

TEST(IdealCacheTest, UniqueRequestsHaveNoHits)
{
    const std::vector<int> requests = {1, 2, 3, 4, 5};

    EXPECT_EQ(caches::CountIdealHits(2, requests), 0u);
}

TEST(IdealCacheTest, EvictsPageWithFarthestNextRequest)
{
    const std::vector<int> requests = {1, 2, 3, 1, 2, 3};

    EXPECT_EQ(caches::CountIdealHits(2, requests), 2u);
}

TEST(IdealCacheTest, EvictsPageThatIsNeverRequestedAgain)
{
    const std::vector<int> requests = {1, 2, 3, 2, 3};

    EXPECT_EQ(caches::CountIdealHits(2, requests), 2u);
}

TEST(IdealCacheTest, HitsAdvanceFuturePositions)
{
    const std::vector<int> requests = {1, 2, 1, 3, 2, 3, 1};

    EXPECT_EQ(caches::CountIdealHits(2, requests), 3u);
}

TEST(IdealCacheTest, CapacityOneLoadsEveryMissingPage)
{
    const std::vector<int> requests = {1, 2, 1};

    EXPECT_EQ(caches::CountIdealHits(1, requests), 0u);
}

TEST(IdealCacheTest, SupportsStringKeys)
{
    const std::vector<std::string> requests = {"a", "b", "c", "a", "b", "c"};

    EXPECT_EQ(caches::CountIdealHits(2, requests), 2u);
}
