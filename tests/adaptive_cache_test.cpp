#include "arc.hpp"
#include "lirs.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using Log = std::vector<std::pair<int, int>>;

    int LoadAdaptivePage(int key) { return key * 100; }
    int RejectAdaptiveLoad(int) { throw std::runtime_error("Unexpected load on hit"); }
    void RecordAdaptiveEviction(int key, int page, Log* log) { log->emplace_back(key, page); }
    void ThrowAdaptiveEviction(int, int, Log*) { throw std::runtime_error("Callback failed"); }
    void IgnoreAdaptiveString(std::string, std::string, int*) {}
    void IgnoreAdaptiveShared(int, std::shared_ptr<int>, int*) {}

    struct CopyPage
    {
        int value;
        inline static bool reject_copy = false;
        explicit CopyPage(int number) : value(number) {}
        CopyPage(const CopyPage& other) : value(other.value)
        {
            if (reject_copy && value == 99)
                throw std::runtime_error("Page copy failed");
        }
        CopyPage(CopyPage&&) = default;
    };

    void IgnoreAdaptiveCopy(int, CopyPage, int*) {}

    template <typename Cache>
    void CheckCopyFailure()
    {
        Cache cache(1);
        cache.insert(1, CopyPage{10}, IgnoreAdaptiveCopy, static_cast<int*>(nullptr));
        CopyPage::reject_copy = true;
        EXPECT_THROW(cache.insert(2, CopyPage{99}, IgnoreAdaptiveCopy,
                                 static_cast<int*>(nullptr)), std::runtime_error);
        CopyPage::reject_copy = false;
        EXPECT_EQ(cache.size(), 1u);
        EXPECT_FALSE(cache.touch(2));
        auto page = cache.extract(1);
        ASSERT_TRUE(page.has_value());
        EXPECT_EQ(page->value, 10);
        cache.insert(2, CopyPage{20}, IgnoreAdaptiveCopy, static_cast<int*>(nullptr));
        EXPECT_TRUE(cache.touch(2));
    }

    template <typename Cache>
    void CheckStringTypes()
    {
        Cache cache(2);
        cache.insert("alpha", "first", IgnoreAdaptiveString, static_cast<int*>(nullptr));
        cache.insert("beta", "second", IgnoreAdaptiveString, static_cast<int*>(nullptr));
        EXPECT_TRUE(cache.touch("alpha"));
        cache.insert("gamma", "third", IgnoreAdaptiveString, static_cast<int*>(nullptr));
        EXPECT_EQ(cache.extract("alpha"), std::optional<std::string>{"first"});
        EXPECT_EQ(cache.extract("gamma"), std::optional<std::string>{"third"});
        EXPECT_TRUE(cache.empty());
    }
}

template <typename Cache>
class AdaptiveCacheTest : public testing::Test {};

using AdaptiveCaches = testing::Types<caches::ARCCache<int>, caches::LIRSCache<int>>;
TYPED_TEST_SUITE(AdaptiveCacheTest, AdaptiveCaches);

TYPED_TEST(AdaptiveCacheTest, StartsEmpty)
{
    TypeParam cache(3);
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_EQ(cache.capacity(), 3u);
    EXPECT_TRUE(cache.empty());
    EXPECT_FALSE(cache.full());
    EXPECT_FALSE(cache.touch(1));
    EXPECT_FALSE(cache.extract(1).has_value());
}

TYPED_TEST(AdaptiveCacheTest, ZeroCapacityForwardsEveryPage)
{
    TypeParam cache(0);
    Log log;
    EXPECT_FALSE(cache.lookup_update(7, LoadAdaptivePage, RecordAdaptiveEviction, &log));
    EXPECT_FALSE(cache.lookup_update(7, LoadAdaptivePage, RecordAdaptiveEviction, &log));
    const Log expected{{7, 700}, {7, 700}};
    EXPECT_EQ(log, expected);
    EXPECT_TRUE(cache.empty());
    EXPECT_TRUE(cache.full());
    EXPECT_FALSE(cache.touch(7));
    EXPECT_FALSE(cache.extract(7).has_value());
}

TYPED_TEST(AdaptiveCacheTest, ResidentHitAvoidsLoadingAndPreservesPayload)
{
    TypeParam cache(2);
    Log log;
    cache.insert(7, 123, RecordAdaptiveEviction, &log);
    for (int repeat = 0; repeat < 20; ++repeat)
        EXPECT_TRUE(cache.lookup_update(7, RejectAdaptiveLoad, RecordAdaptiveEviction, &log));
    EXPECT_TRUE(log.empty());
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_EQ(cache.extract(7), std::optional<int>{123});
    EXPECT_TRUE(cache.empty());
}

TYPED_TEST(AdaptiveCacheTest, CapacityOneReplacesResident)
{
    TypeParam cache(1);
    Log log;
    for (int key : {1, 2, 1, 3})
        EXPECT_FALSE(cache.lookup_update(key, LoadAdaptivePage, RecordAdaptiveEviction, &log));
    const Log expected{{1, 100}, {2, 200}, {1, 100}};
    EXPECT_EQ(log, expected);
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_EQ(cache.extract(3), std::optional<int>{300});
}

TYPED_TEST(AdaptiveCacheTest, ExtractsAllPagesAndReusesCapacity)
{
    TypeParam cache(3);
    Log log;
    for (int key : {1, 2, 3})
        cache.insert(key, key + 10, RecordAdaptiveEviction, &log);
    for (int key : {1, 2, 3})
    {
        EXPECT_EQ(cache.extract(key), std::optional<int>{key + 10});
        EXPECT_FALSE(cache.extract(key).has_value());
    }
    EXPECT_TRUE(cache.empty());
    for (int key : {4, 5, 6})
        cache.insert(key, key + 10, RecordAdaptiveEviction, &log);
    EXPECT_TRUE(log.empty());
    EXPECT_TRUE(cache.full());
}

TYPED_TEST(AdaptiveCacheTest, DuplicateInsertionPreservesOriginalPage)
{
    TypeParam cache(1);
    Log log;
    cache.insert(1, 123, RecordAdaptiveEviction, &log);
    EXPECT_THROW(cache.insert(1, 456, RecordAdaptiveEviction, &log), std::invalid_argument);
    EXPECT_TRUE(log.empty());
    EXPECT_EQ(cache.extract(1), std::optional<int>{123});
}

TYPED_TEST(AdaptiveCacheTest, ThrowingCallbackLeavesCommittedLocalState)
{
    TypeParam cache(1);
    Log log;
    cache.insert(1, 100, RecordAdaptiveEviction, &log);
    EXPECT_THROW(cache.insert(2, 200, ThrowAdaptiveEviction, &log), std::runtime_error);
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_FALSE(cache.touch(1));
    EXPECT_EQ(cache.extract(2), std::optional<int>{200});
}

TEST(ARCCacheTest, AdaptsToBothGhostLists)
{
    caches::ARCCache<int> cache(2);
    Log log;
    int hits = 0;
    for (int key : {1, 2, 1, 3, 2, 1, 4, 3, 2, 5})
        hits += cache.lookup_update(key, LoadAdaptivePage, RecordAdaptiveEviction, &log);
    const Log expected{{2, 200}, {1, 100}, {3, 300}, {2, 200}, {1, 100}, {4, 400}, {3, 300}};
    EXPECT_EQ(hits, 1);
    EXPECT_EQ(log, expected);
    EXPECT_EQ(cache.extract(2), std::optional<int>{200});
    EXPECT_EQ(cache.extract(5), std::optional<int>{500});
    EXPECT_TRUE(cache.empty());
}

TEST(ARCCacheTest, B2ReloadAtTargetEvictsFromT1)
{
    caches::ARCCache<int> cache(3);
    Log log;
    for (int key : {1, 2, 3, 1, 1, 5, 0, 0, 1, 1, 4, 1, 3, 5, 0})
        cache.lookup_update(key, LoadAdaptivePage, RecordAdaptiveEviction, &log);
    const Log expected{{2, 200}, {3, 300}, {5, 500}, {0, 0}, {1, 100}, {4, 400}};
    EXPECT_EQ(log, expected);
    for (int key : {0, 3, 5})
        EXPECT_TRUE(cache.touch(key));
    EXPECT_FALSE(cache.touch(4));
}

TEST(ARCCacheTest, ColdScanDiscardsT1WithoutFalseGhostPromotion)
{
    caches::ARCCache<int> cache(2);
    Log log;
    for (int key : {1, 2, 3, 1})
        cache.lookup_update(key, LoadAdaptivePage, RecordAdaptiveEviction, &log);
    EXPECT_TRUE(cache.touch(3));
    cache.lookup_update(4, LoadAdaptivePage, RecordAdaptiveEviction, &log);
    EXPECT_EQ(log.back(), std::make_pair(1, 100));
    EXPECT_TRUE(cache.touch(3));
    EXPECT_TRUE(cache.touch(4));
}

TEST(ARCCacheTest, GhostIsMissAndUsesReloadedPayload)
{
    caches::ARCCache<int> cache(2);
    Log log;
    cache.insert(1, 111, RecordAdaptiveEviction, &log);
    cache.insert(2, 222, RecordAdaptiveEviction, &log);
    cache.touch(1);
    cache.insert(3, 333, RecordAdaptiveEviction, &log);
    EXPECT_FALSE(cache.touch(2));
    EXPECT_FALSE(cache.extract(2).has_value());
    cache.insert(2, 999, RecordAdaptiveEviction, &log);
    EXPECT_EQ(cache.extract(2), std::optional<int>{999});
    EXPECT_EQ(log.back(), std::make_pair(1, 111));
}

TEST(ARCCacheTest, ExtractionAllowsReloadIntoFreeSpaceWithHistory)
{
    caches::ARCCache<int> cache(2);
    Log log;
    for (int key : {1, 2, 1, 3})
        cache.lookup_update(key, LoadAdaptivePage, RecordAdaptiveEviction, &log);
    EXPECT_EQ(cache.extract(1), std::optional<int>{100});
    const auto before = log.size();
    cache.insert(2, 222, RecordAdaptiveEviction, &log);
    EXPECT_EQ(log.size(), before);
    EXPECT_EQ(cache.extract(2), std::optional<int>{222});
    EXPECT_EQ(cache.extract(3), std::optional<int>{300});
}

TEST(LIRSCacheTest, PromotesRememberedHirAndDemotesBottomLir)
{
    caches::LIRSCache<int> cache(3);
    Log log;
    int hits = 0;
    for (int key : {1, 2, 3, 4, 3, 5, 5, 2, 6})
        hits += cache.lookup_update(key, LoadAdaptivePage, RecordAdaptiveEviction, &log);
    const Log expected{{3, 300}, {4, 400}, {1, 100}, {2, 200}};
    EXPECT_EQ(hits, 2);
    EXPECT_EQ(log, expected);
    for (int key : {3, 5, 6})
        EXPECT_TRUE(cache.touch(key));
}

TEST(LIRSCacheTest, PrunedResidentHirDoesNotPromoteOnFirstReturn)
{
    caches::LIRSCache<int> cache(3);
    Log log;
    for (int key : {1, 2, 3, 1, 2, 3, 4})
        cache.lookup_update(key, LoadAdaptivePage, RecordAdaptiveEviction, &log);
    const Log expected{{3, 300}};
    EXPECT_EQ(log, expected);
    EXPECT_TRUE(cache.touch(1));
    EXPECT_TRUE(cache.touch(2));
    EXPECT_TRUE(cache.touch(4));
}

TEST(LIRSCacheTest, StackLimitForgetsOldNonresidentHir)
{
    caches::LIRSCache<int> cache(2);
    Log log;
    for (int key : {1, 2, 3, 4, 5, 2, 6})
        cache.lookup_update(key, LoadAdaptivePage, RecordAdaptiveEviction, &log);
    const Log expected{{2, 200}, {3, 300}, {4, 400}, {5, 500}, {2, 200}};
    EXPECT_EQ(log, expected);
    EXPECT_TRUE(cache.touch(1));
    EXPECT_TRUE(cache.touch(6));
}

TEST(LIRSCacheTest, ExtractedLirSlotsCanBeRefilled)
{
    caches::LIRSCache<int> cache(3);
    Log log;
    for (int key : {1, 2, 3})
        cache.lookup_update(key, LoadAdaptivePage, RecordAdaptiveEviction, &log);
    EXPECT_EQ(cache.extract(1), std::optional<int>{100});
    cache.lookup_update(4, LoadAdaptivePage, RecordAdaptiveEviction, &log);
    cache.lookup_update(5, LoadAdaptivePage, RecordAdaptiveEviction, &log);
    EXPECT_EQ(cache.extract(2), std::optional<int>{200});
    EXPECT_TRUE(cache.touch(5));
    cache.lookup_update(6, LoadAdaptivePage, RecordAdaptiveEviction, &log);
    cache.lookup_update(7, LoadAdaptivePage, RecordAdaptiveEviction, &log);
    const Log expected{{3, 300}, {6, 600}};
    EXPECT_EQ(log, expected);
    for (int key : {4, 5, 7})
        EXPECT_TRUE(cache.extract(key).has_value());
    EXPECT_TRUE(cache.empty());
}

TEST(AdaptiveCacheTypesTest, SupportsStringKeysAndPages)
{
    CheckStringTypes<caches::ARCCache<std::string, std::string>>();
    CheckStringTypes<caches::LIRSCache<std::string, std::string>>();
}

TEST(AdaptiveCacheTypesTest, FailedCopyPreservesOldPage)
{
    CheckCopyFailure<caches::ARCCache<CopyPage>>();
    CheckCopyFailure<caches::LIRSCache<CopyPage>>();
}

TEST(AdaptiveCacheTypesTest, HistoryDoesNotKeepPayloadAlive)
{
    caches::ARCCache<std::shared_ptr<int>> arc(2);
    caches::LIRSCache<std::shared_ptr<int>> lirs(2);
    auto page = std::make_shared<int>(123);
    std::weak_ptr<int> observer = page;
    arc.insert(1, std::make_shared<int>(1), IgnoreAdaptiveShared, static_cast<int*>(nullptr));
    lirs.insert(1, std::make_shared<int>(1), IgnoreAdaptiveShared, static_cast<int*>(nullptr));
    arc.insert(2, page, IgnoreAdaptiveShared, static_cast<int*>(nullptr));
    lirs.insert(2, page, IgnoreAdaptiveShared, static_cast<int*>(nullptr));
    page.reset();
    arc.touch(1);
    arc.insert(3, std::make_shared<int>(3), IgnoreAdaptiveShared, static_cast<int*>(nullptr));
    EXPECT_FALSE(observer.expired());
    lirs.insert(3, std::make_shared<int>(3), IgnoreAdaptiveShared, static_cast<int*>(nullptr));
    EXPECT_TRUE(observer.expired());
    EXPECT_FALSE(arc.touch(2));
    EXPECT_FALSE(lirs.touch(2));
}

TEST(LIRSCacheTest, ReservesOnePercentForResidentHir)
{
    caches::LIRSCache<int> cache(200);
    Log log;
    for (int key = 1; key <= 201; ++key)
        cache.lookup_update(key, LoadAdaptivePage, RecordAdaptiveEviction, &log);
    EXPECT_TRUE(cache.touch(200));
    cache.lookup_update(202, LoadAdaptivePage, RecordAdaptiveEviction, &log);
    const Log expected{{199, 19900}, {201, 20100}};
    EXPECT_EQ(log, expected);
    EXPECT_EQ(cache.size(), 200u);
    EXPECT_TRUE(cache.touch(1));
    EXPECT_TRUE(cache.touch(200));
    EXPECT_TRUE(cache.touch(202));
}
