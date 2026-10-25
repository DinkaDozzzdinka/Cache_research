#include "two_q.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using EvictionLog = std::vector<std::pair<int, int>>;

    int LoadTwoQPage(int key)
    {
        return key * 100;
    }

    int RejectTwoQLoad(int)
    {
        throw std::runtime_error("A resident page must not be loaded again");
    }

    void RecordTwoQEviction(int key, int page, EvictionLog* log)
    {
        log->emplace_back(key, page);
    }

    void IgnoreStringEviction(std::string, std::string, int*)
    {}

    void IgnoreSharedEviction(int, std::shared_ptr<int>, int*)
    {}

    struct CopyCheckedPage
    {
        int value;
        inline static bool reject_copy = false;

        explicit CopyCheckedPage(int number) : value(number)
        {}

        CopyCheckedPage(const CopyCheckedPage& other) : value(other.value)
        {
            if (reject_copy && value == 99)
                throw std::runtime_error("Page copy failed");
        }

        CopyCheckedPage(CopyCheckedPage&&) = default;
    };

    void IgnoreCheckedEviction(int, CopyCheckedPage, int*)
    {}
}

TEST(TwoQCacheTest, StartsEmpty)
{
    caches::TwoQCache<int> cache(3);

    EXPECT_EQ(cache.capacity(), 3u);
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_TRUE(cache.empty());
    EXPECT_FALSE(cache.full());
    EXPECT_FALSE(cache.touch(1));
    EXPECT_FALSE(cache.extract(1).has_value());
}

TEST(TwoQCacheTest, ZeroCapacityForwardsEveryPage)
{
    caches::TwoQCache<int> cache(0);
    EvictionLog log;

    EXPECT_FALSE(cache.lookup_update(7, LoadTwoQPage, RecordTwoQEviction, &log));
    EXPECT_FALSE(cache.lookup_update(7, LoadTwoQPage, RecordTwoQEviction, &log));

    const EvictionLog expected{{7, 700}, {7, 700}};
    EXPECT_EQ(log, expected);
    EXPECT_TRUE(cache.empty());
    EXPECT_TRUE(cache.full());
    EXPECT_FALSE(cache.touch(7));
    EXPECT_FALSE(cache.extract(7).has_value());
}

TEST(TwoQCacheTest, HitsInA1inPreserveFifoOrder)
{
    caches::TwoQCache<int> cache(3);
    EvictionLog log;

    for (int key : {1, 2, 3})
        cache.lookup_update(key, LoadTwoQPage, RecordTwoQEviction, &log);
    EXPECT_TRUE(cache.lookup_update(1, RejectTwoQLoad, RecordTwoQEviction, &log));
    cache.lookup_update(4, LoadTwoQPage, RecordTwoQEviction, &log);

    const EvictionLog expected{{1, 100}};
    EXPECT_EQ(log, expected);
    EXPECT_EQ(cache.size(), 3u);
    EXPECT_FALSE(cache.touch(1));
    EXPECT_TRUE(cache.touch(2));
    EXPECT_TRUE(cache.touch(3));
    EXPECT_TRUE(cache.touch(4));
}

TEST(TwoQCacheTest, GhostMissReloadsIntoMainAndSurvivesScan)
{
    caches::TwoQCache<int> cache(3);
    EvictionLog log;

    for (int key : {1, 2, 3, 4})
        EXPECT_FALSE(cache.lookup_update(key, LoadTwoQPage, RecordTwoQEviction, &log));
    EXPECT_FALSE(cache.touch(1));
    EXPECT_FALSE(cache.lookup_update(1, LoadTwoQPage, RecordTwoQEviction, &log));
    EXPECT_TRUE(cache.lookup_update(1, RejectTwoQLoad, RecordTwoQEviction, &log));
    cache.lookup_update(5, LoadTwoQPage, RecordTwoQEviction, &log);
    cache.lookup_update(6, LoadTwoQPage, RecordTwoQEviction, &log);

    const EvictionLog expected{{1, 100}, {2, 200}, {3, 300}, {4, 400}};
    EXPECT_EQ(log, expected);
    EXPECT_EQ(cache.extract(1), std::optional<int>{100});
    EXPECT_EQ(cache.extract(5), std::optional<int>{500});
    EXPECT_EQ(cache.extract(6), std::optional<int>{600});
    EXPECT_TRUE(cache.empty());
}

TEST(TwoQCacheTest, UsesLruOrderInsideMain)
{
    caches::TwoQCache<int> cache(3);
    EvictionLog log;

    for (int key : {1, 2, 3, 4, 1, 2, 3})
        cache.lookup_update(key, LoadTwoQPage, RecordTwoQEviction, &log);
    EXPECT_TRUE(cache.touch(2));
    cache.lookup_update(5, LoadTwoQPage, RecordTwoQEviction, &log);

    const EvictionLog expected{{1, 100}, {2, 200}, {3, 300}, {1, 100}, {3, 300}};
    EXPECT_EQ(log, expected);
    EXPECT_TRUE(cache.touch(2));
    EXPECT_FALSE(cache.touch(3));
    EXPECT_TRUE(cache.touch(4));
    EXPECT_TRUE(cache.touch(5));
}

TEST(TwoQCacheTest, EvictsFromMainWhenA1inEqualsTarget)
{
    caches::TwoQCache<int> cache(2);
    EvictionLog log;

    for (int key : {1, 2, 3, 1, 4})
        cache.lookup_update(key, LoadTwoQPage, RecordTwoQEviction, &log);

    const EvictionLog expected{{1, 100}, {2, 200}, {1, 100}};
    EXPECT_EQ(log, expected);
    EXPECT_TRUE(cache.touch(3));
    EXPECT_TRUE(cache.touch(4));
}

TEST(TwoQCacheTest, MainEvictionsDoNotEnterHistory)
{
    caches::TwoQCache<int> cache(3);
    EvictionLog log;

    for (int key : {1, 2, 3, 4, 1, 2, 3})
        cache.lookup_update(key, LoadTwoQPage, RecordTwoQEviction, &log);
    cache.touch(2);
    for (int key : {5, 3, 6})
        cache.lookup_update(key, LoadTwoQPage, RecordTwoQEviction, &log);

    ASSERT_EQ(log.size(), 7u);
    EXPECT_EQ(log.back(), std::make_pair(5, 500));
    EXPECT_TRUE(cache.touch(2));
    EXPECT_TRUE(cache.touch(3));
    EXPECT_TRUE(cache.touch(6));
}

TEST(TwoQCacheTest, ForgetsOldestGhostWhenHistoryIsFull)
{
    caches::TwoQCache<int> cache(2);
    EvictionLog log;

    for (int key : {1, 2, 3, 4, 1, 5})
        cache.lookup_update(key, LoadTwoQPage, RecordTwoQEviction, &log);

    const EvictionLog expected{{1, 100}, {2, 200}, {3, 300}, {4, 400}};
    EXPECT_EQ(log, expected);
    EXPECT_EQ(cache.size(), 2u);
    EXPECT_TRUE(cache.touch(1));
    EXPECT_TRUE(cache.touch(5));
}

TEST(TwoQCacheTest, CapacityOneHandlesColdAndGhostMisses)
{
    caches::TwoQCache<int> cache(1);
    EvictionLog log;

    EXPECT_FALSE(cache.lookup_update(1, LoadTwoQPage, RecordTwoQEviction, &log));
    EXPECT_TRUE(cache.lookup_update(1, RejectTwoQLoad, RecordTwoQEviction, &log));
    for (int key : {2, 1, 3})
        EXPECT_FALSE(cache.lookup_update(key, LoadTwoQPage, RecordTwoQEviction, &log));

    const EvictionLog expected{{1, 100}, {2, 200}, {1, 100}};
    EXPECT_EQ(log, expected);
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_EQ(cache.extract(3), std::optional<int>{300});
    EXPECT_TRUE(cache.empty());
}

TEST(TwoQCacheTest, ExtractingResidentPageDoesNotCreateGhost)
{
    caches::TwoQCache<int> cache(2);
    EvictionLog log;

    cache.insert(1, 101, RecordTwoQEviction, &log);
    cache.insert(2, 202, RecordTwoQEviction, &log);
    EXPECT_EQ(cache.extract(1), std::optional<int>{101});
    EXPECT_FALSE(cache.full());
    cache.insert(1, 303, RecordTwoQEviction, &log);
    cache.insert(3, 404, RecordTwoQEviction, &log);

    const EvictionLog expected{{2, 202}};
    EXPECT_EQ(log, expected);
    EXPECT_EQ(cache.extract(1), std::optional<int>{303});
    EXPECT_EQ(cache.extract(3), std::optional<int>{404});
    EXPECT_TRUE(cache.empty());
}

TEST(TwoQCacheTest, ExtractsMainPageAndReusesCapacity)
{
    caches::TwoQCache<int> cache(3);
    EvictionLog log;

    for (int key : {1, 2, 3, 4, 1})
        cache.lookup_update(key, LoadTwoQPage, RecordTwoQEviction, &log);
    EXPECT_EQ(cache.extract(1), std::optional<int>{100});
    EXPECT_EQ(cache.extract(3), std::optional<int>{300});
    EXPECT_EQ(cache.size(), 1u);
    const auto evictions_before = log.size();
    cache.insert(5, 505, RecordTwoQEviction, &log);
    cache.insert(6, 606, RecordTwoQEviction, &log);

    EXPECT_EQ(log.size(), evictions_before);
    EXPECT_TRUE(cache.full());
    EXPECT_EQ(cache.extract(4), std::optional<int>{400});
    EXPECT_EQ(cache.extract(5), std::optional<int>{505});
    EXPECT_EQ(cache.extract(6), std::optional<int>{606});
    EXPECT_TRUE(cache.empty());
}

TEST(TwoQCacheTest, MissingExtractionPreservesGhostHistory)
{
    caches::TwoQCache<int> cache(2);
    EvictionLog log;

    for (int key : {1, 2, 3})
        cache.lookup_update(key, LoadTwoQPage, RecordTwoQEviction, &log);
    EXPECT_FALSE(cache.extract(1).has_value());
    EXPECT_FALSE(cache.extract(99).has_value());
    for (int key : {1, 4})
        cache.lookup_update(key, LoadTwoQPage, RecordTwoQEviction, &log);

    ASSERT_EQ(log.size(), 3u);
    EXPECT_EQ(log.back(), std::make_pair(1, 100));
    EXPECT_TRUE(cache.touch(3));
    EXPECT_TRUE(cache.touch(4));
}

TEST(TwoQCacheTest, GhostContainsNoPageAndReloadUsesNewPayload)
{
    caches::TwoQCache<int> cache(2);
    EvictionLog log;

    cache.insert(7, 123, RecordTwoQEviction, &log);
    cache.insert(8, 456, RecordTwoQEviction, &log);
    cache.insert(9, 789, RecordTwoQEviction, &log);
    EXPECT_FALSE(cache.extract(7).has_value());
    cache.insert(7, 999, RecordTwoQEviction, &log);

    const EvictionLog expected{{7, 123}, {8, 456}};
    EXPECT_EQ(log, expected);
    EXPECT_EQ(cache.extract(7), std::optional<int>{999});
}

TEST(TwoQCacheTest, GhostDoesNotKeepPageAlive)
{
    caches::TwoQCache<std::shared_ptr<int>> cache(1);
    auto page = std::make_shared<int>(123);
    std::weak_ptr<int> observer = page;

    cache.insert(1, page, IgnoreSharedEviction, static_cast<int*>(nullptr));
    page.reset();
    EXPECT_FALSE(observer.expired());
    cache.insert(2, std::make_shared<int>(456), IgnoreSharedEviction, static_cast<int*>(nullptr));

    EXPECT_TRUE(observer.expired());
    EXPECT_FALSE(cache.touch(1));
    EXPECT_EQ(cache.size(), 1u);
}

TEST(TwoQCacheTest, SupportsStringKeysAndPages)
{
    caches::TwoQCache<std::string, std::string> cache(2);

    cache.insert("alpha", "first", IgnoreStringEviction, static_cast<int*>(nullptr));
    cache.insert("beta", "second", IgnoreStringEviction, static_cast<int*>(nullptr));
    cache.insert("gamma", "third", IgnoreStringEviction, static_cast<int*>(nullptr));
    EXPECT_FALSE(cache.touch("alpha"));
    cache.insert("alpha", "reloaded", IgnoreStringEviction, static_cast<int*>(nullptr));

    EXPECT_EQ(cache.extract("alpha"), std::optional<std::string>{"reloaded"});
    EXPECT_EQ(cache.extract("gamma"), std::optional<std::string>{"third"});
    EXPECT_TRUE(cache.empty());
}

TEST(TwoQCacheTest, RejectsDuplicateResidentInsertion)
{
    caches::TwoQCache<int> cache(1);
    EvictionLog log;
    cache.insert(1, 123, RecordTwoQEviction, &log);

    EXPECT_THROW(cache.insert(1, 456, RecordTwoQEviction, &log), std::invalid_argument);

    EXPECT_TRUE(log.empty());
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_EQ(cache.extract(1), std::optional<int>{123});
}

TEST(TwoQCacheTest, FailedNewPageCopyPreservesVictimAndQueue)
{
    caches::TwoQCache<CopyCheckedPage> cache(1);
    cache.insert(1, CopyCheckedPage{10}, IgnoreCheckedEviction, static_cast<int*>(nullptr));
    CopyCheckedPage::reject_copy = true;

    EXPECT_THROW(cache.insert(2, CopyCheckedPage{99}, IgnoreCheckedEviction,
                             static_cast<int*>(nullptr)), std::runtime_error);

    CopyCheckedPage::reject_copy = false;
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_FALSE(cache.touch(2));
    auto page = cache.extract(1);
    ASSERT_TRUE(page.has_value());
    EXPECT_EQ(page->value, 10);
    cache.insert(2, CopyCheckedPage{20}, IgnoreCheckedEviction, static_cast<int*>(nullptr));
    EXPECT_TRUE(cache.touch(2));
}

TEST(TwoQCacheTest, FailedGhostReloadPreservesHistoryAndResident)
{
    caches::TwoQCache<CopyCheckedPage> cache(2);
    for (int key : {1, 2, 3})
        cache.insert(key, CopyCheckedPage{key * 10}, IgnoreCheckedEviction, static_cast<int*>(nullptr));
    CopyCheckedPage::reject_copy = true;

    EXPECT_THROW(cache.insert(1, CopyCheckedPage{99}, IgnoreCheckedEviction,
                             static_cast<int*>(nullptr)), std::runtime_error);

    CopyCheckedPage::reject_copy = false;
    EXPECT_EQ(cache.size(), 2u);
    EXPECT_FALSE(cache.touch(1));
    EXPECT_TRUE(cache.touch(2));
    EXPECT_TRUE(cache.touch(3));
    cache.insert(1, CopyCheckedPage{40}, IgnoreCheckedEviction, static_cast<int*>(nullptr));
    cache.insert(4, CopyCheckedPage{50}, IgnoreCheckedEviction, static_cast<int*>(nullptr));
    EXPECT_FALSE(cache.touch(1));
    EXPECT_TRUE(cache.touch(3));
    EXPECT_TRUE(cache.touch(4));
}

TEST(TwoQCacheTest, ReloadingNewestGhostPreservesOtherHistoryEntries)
{
    caches::TwoQCache<int> cache(4);
    EvictionLog log;

    for (int key : {1, 2, 3, 4, 5, 6, 2, 1, 7, 8, 9})
        cache.lookup_update(key, LoadTwoQPage, RecordTwoQEviction, &log);

    const EvictionLog expected{{1, 100}, {2, 200}, {3, 300}, {4, 400},
                               {5, 500}, {6, 600}, {7, 700}};
    EXPECT_EQ(log, expected);
    EXPECT_EQ(cache.size(), 4u);
    for (int key : {1, 2, 8, 9})
        EXPECT_TRUE(cache.touch(key));
}

TEST(TwoQCacheTest, UsesQuarterCapacityAsA1inTarget)
{
    caches::TwoQCache<int> cache(8);
    EvictionLog log;

    for (int key : {1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2, 3, 4, 5, 6, 7})
        cache.lookup_update(key, LoadTwoQPage, RecordTwoQEviction, &log);

    ASSERT_EQ(log.size(), 8u);
    EXPECT_EQ(log.back(), std::make_pair(1, 100));
    EXPECT_EQ(cache.size(), 8u);
    EXPECT_FALSE(cache.touch(1));
    for (int key : {2, 3, 4, 5, 6, 7, 8, 9})
        EXPECT_TRUE(cache.touch(key));
}
