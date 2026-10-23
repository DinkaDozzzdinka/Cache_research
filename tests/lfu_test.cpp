#include "lfu.hpp"
#include <gtest/gtest.h>
#include <stdexcept>

struct LFUEvictionLog {
    int calls = 0;
    int last_key = 0;
    int last_page = 0;
};

int LoadLFUTestPage(int key) {
    return key * 100;
}

int RejectLFUTestPageLoad(int) {
    throw std::runtime_error("A cache hit must not load the page");
}

void RecordLFUEviction(int key, int page, LFUEvictionLog* log) {
    ++log->calls;
    log->last_key = key;
    log->last_page = page;
}

TEST(LFUCacheTest, StartsEmpty) {
    caches::LFUCache<int> cache(2);

    EXPECT_TRUE(cache.empty());
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_EQ(cache.capacity(), 2u);
    EXPECT_FALSE(cache.full());
    EXPECT_FALSE(cache.extract(7).has_value());
}

TEST(LFUCacheTest, EvictsLeastFrequentlyUsedPage) {
    caches::LFUCache<int> cache(2);
    LFUEvictionLog log;

    EXPECT_FALSE(cache.lookup_update(1, LoadLFUTestPage, RecordLFUEviction, &log));
    EXPECT_FALSE(cache.lookup_update(2, LoadLFUTestPage, RecordLFUEviction, &log));
    EXPECT_TRUE(cache.lookup_update(1, LoadLFUTestPage, RecordLFUEviction, &log));
    EXPECT_FALSE(cache.lookup_update(3, LoadLFUTestPage, RecordLFUEviction, &log));

    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(log.calls, 1);
    EXPECT_EQ(log.last_key, 2);
    EXPECT_EQ(log.last_page, 200);
    EXPECT_FALSE(cache.extract(2).has_value());

    auto first = cache.extract(1);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, 100);

    auto third = cache.extract(3);
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(*third, 300);
    EXPECT_TRUE(cache.empty());
}

TEST(LFUCacheTest, EvictsLeastRecentlyUsedWhenFrequenciesMatch) {
    caches::LFUCache<int> cache(2);
    LFUEvictionLog log;

    EXPECT_FALSE(cache.lookup_update(1, LoadLFUTestPage, RecordLFUEviction, &log));
    EXPECT_FALSE(cache.lookup_update(2, LoadLFUTestPage, RecordLFUEviction, &log));
    EXPECT_TRUE(cache.lookup_update(1, LoadLFUTestPage, RecordLFUEviction, &log));
    EXPECT_TRUE(cache.lookup_update(2, LoadLFUTestPage, RecordLFUEviction, &log));
    EXPECT_FALSE(cache.lookup_update(3, LoadLFUTestPage, RecordLFUEviction, &log));

    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(log.calls, 1);
    EXPECT_EQ(log.last_key, 1);
    EXPECT_EQ(log.last_page, 100);
    EXPECT_FALSE(cache.extract(1).has_value());

    auto second = cache.extract(2);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, 200);

    auto third = cache.extract(3);
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(*third, 300);
    EXPECT_TRUE(cache.empty());
}

TEST(LFUCacheTest, EvictsByFrequencyThenRecency) {
    caches::LFUCache<int> cache(3);
    LFUEvictionLog log;

    EXPECT_FALSE(cache.lookup_update(1, LoadLFUTestPage, RecordLFUEviction, &log));
    EXPECT_FALSE(cache.lookup_update(2, LoadLFUTestPage, RecordLFUEviction, &log));
    EXPECT_FALSE(cache.lookup_update(3, LoadLFUTestPage, RecordLFUEviction, &log));
    EXPECT_TRUE(cache.lookup_update(1, LoadLFUTestPage, RecordLFUEviction, &log));
    EXPECT_TRUE(cache.lookup_update(2, LoadLFUTestPage, RecordLFUEviction, &log));
    EXPECT_FALSE(cache.lookup_update(4, LoadLFUTestPage, RecordLFUEviction, &log));

    EXPECT_EQ(log.calls, 1);
    EXPECT_EQ(log.last_key, 3);
    EXPECT_EQ(log.last_page, 300);

    EXPECT_TRUE(cache.lookup_update(4, LoadLFUTestPage, RecordLFUEviction, &log));
    EXPECT_FALSE(cache.lookup_update(5, LoadLFUTestPage, RecordLFUEviction, &log));

    EXPECT_EQ(cache.size(), 3u);
    EXPECT_EQ(log.calls, 2);
    EXPECT_EQ(log.last_key, 1);
    EXPECT_EQ(log.last_page, 100);
    EXPECT_FALSE(cache.extract(1).has_value());
    EXPECT_FALSE(cache.extract(3).has_value());

    for (int key : {2, 4, 5}) {
        auto page = cache.extract(key);
        ASSERT_TRUE(page.has_value());
        EXPECT_EQ(*page, key * 100);
    }
    EXPECT_TRUE(cache.empty());
}

TEST(LFUCacheTest, RepeatedHitsPreservePageAndAvoidLoading) {
    caches::LFUCache<int> cache(1);
    LFUEvictionLog log;

    cache.insert(7, 123, RecordLFUEviction, &log);
    for (int i = 0; i < 20; ++i) {
        EXPECT_TRUE(cache.lookup_update(7, RejectLFUTestPageLoad, RecordLFUEviction, &log));
    }

    EXPECT_EQ(cache.size(), 1u);
    EXPECT_TRUE(cache.full());
    EXPECT_EQ(log.calls, 0);

    EXPECT_FALSE(cache.lookup_update(8, LoadLFUTestPage, RecordLFUEviction, &log));

    EXPECT_EQ(log.calls, 1);
    EXPECT_EQ(log.last_key, 7);
    EXPECT_EQ(log.last_page, 123);
    EXPECT_FALSE(cache.extract(7).has_value());

    auto page = cache.extract(8);
    ASSERT_TRUE(page.has_value());
    EXPECT_EQ(*page, 800);
    EXPECT_TRUE(cache.empty());
}

TEST(LFUCacheTest, MissingExtractionPreservesEvictionOrder) {
    caches::LFUCache<int> cache(2);
    LFUEvictionLog log;

    cache.lookup_update(1, LoadLFUTestPage, RecordLFUEviction, &log);
    cache.lookup_update(2, LoadLFUTestPage, RecordLFUEviction, &log);
    cache.lookup_update(1, LoadLFUTestPage, RecordLFUEviction, &log);
    cache.lookup_update(2, LoadLFUTestPage, RecordLFUEviction, &log);

    EXPECT_FALSE(cache.extract(99).has_value());
    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(log.calls, 0);

    cache.insert(3, 300, RecordLFUEviction, &log);

    EXPECT_EQ(log.calls, 1);
    EXPECT_EQ(log.last_key, 1);
    EXPECT_EQ(log.last_page, 100);
    EXPECT_FALSE(cache.extract(1).has_value());

    auto second = cache.extract(2);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, 200);

    auto third = cache.extract(3);
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(*third, 300);
    EXPECT_TRUE(cache.empty());
}

TEST(LFUCacheTest, ExtractsPromotedPageAndReusesCapacity) {
    caches::LFUCache<int> cache(2);
    LFUEvictionLog log;

    cache.lookup_update(1, LoadLFUTestPage, RecordLFUEviction, &log);
    cache.lookup_update(2, LoadLFUTestPage, RecordLFUEviction, &log);
    cache.lookup_update(1, LoadLFUTestPage, RecordLFUEviction, &log);
    cache.lookup_update(1, LoadLFUTestPage, RecordLFUEviction, &log);

    auto first = cache.extract(1);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, 100);
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_EQ(log.calls, 0);

    EXPECT_TRUE(cache.lookup_update(2, RejectLFUTestPageLoad, RecordLFUEviction, &log));
    EXPECT_FALSE(cache.lookup_update(3, LoadLFUTestPage, RecordLFUEviction, &log));
    EXPECT_EQ(log.calls, 0);

    EXPECT_FALSE(cache.lookup_update(4, LoadLFUTestPage, RecordLFUEviction, &log));

    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(log.calls, 1);
    EXPECT_EQ(log.last_key, 3);
    EXPECT_EQ(log.last_page, 300);
    EXPECT_FALSE(cache.extract(3).has_value());

    auto second = cache.extract(2);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, 200);

    auto fourth = cache.extract(4);
    ASSERT_TRUE(fourth.has_value());
    EXPECT_EQ(*fourth, 400);
    EXPECT_TRUE(cache.empty());
}

TEST(LFUCacheTest, ZeroCapacityHasNoHitsAndForwardsPages) {
    caches::LFUCache<int> cache(0);
    LFUEvictionLog log;

    for (int i = 1; i <= 3; ++i) {
        EXPECT_FALSE(cache.lookup_update(7, LoadLFUTestPage, RecordLFUEviction, &log));
        EXPECT_EQ(log.calls, i);
        EXPECT_EQ(log.last_key, 7);
        EXPECT_EQ(log.last_page, 700);
        EXPECT_TRUE(cache.empty());
        EXPECT_EQ(cache.size(), 0u);
    }

    EXPECT_TRUE(cache.full());
    EXPECT_FALSE(cache.extract(7).has_value());
}
