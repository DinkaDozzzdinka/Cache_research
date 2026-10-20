#include "lru.hpp"
#include <gtest/gtest.h>


TEST(LRUCacheTest, StartsEmpty) 
{
    caches ::LRUCache<int> cache(2);

    EXPECT_TRUE (cache.empty());
    EXPECT_EQ (cache.size(), 0u);
    EXPECT_EQ (cache.capacity(), 2u);
    EXPECT_FALSE (cache.full());
}

TEST(LRUCacheTest, ExtractMissingKey) 
{
    caches ::LRUCache<int> cache(2);
    std::optional<int> page;
    page = cache.extract(7);

    EXPECT_FALSE (page.has_value());
    EXPECT_EQ (cache.size(), 0u);
}

void CallBack (int, int, int *local_counter_callback)
{
    (*local_counter_callback)++;
}

TEST(LRUCacheTest, InsertAndExtractPage) 
{
    caches ::LRUCache<int> cache(2);
    int local_counter_callback = 0;
    cache.insert (7, 123, CallBack, &local_counter_callback);
    EXPECT_EQ (cache.size(), 1u);
    EXPECT_EQ (local_counter_callback, 0u);
    
    std::optional<int> page;
    page = cache.extract(7);

    ASSERT_TRUE (page.has_value());
    EXPECT_EQ (*page, 123);
    EXPECT_EQ (cache.size(), 0u);
}

TEST(LRUCacheTest, InsertAndExtractPageWithCallBack) 
{
    caches ::LRUCache<int> cache(1);
    int local_counter_callback = 0;
    cache.insert (7, 123, CallBack, &local_counter_callback);
    EXPECT_EQ (cache.size(), 1u);
    EXPECT_EQ (local_counter_callback, 0u);

    cache.insert (8, 456, CallBack, &local_counter_callback);
    EXPECT_EQ (cache.size(), 1u);
    EXPECT_EQ (local_counter_callback, 1u);

    std::optional<int> page;
    page = cache.extract(7);
    ASSERT_FALSE (page.has_value());
    EXPECT_EQ (cache.size(), 1u);

    page = cache.extract(8);
    ASSERT_TRUE (page.has_value());
    EXPECT_EQ (*page, 456);
    EXPECT_EQ (cache.size(), 0u);
}

TEST(LRUCacheTest, ZeroCapacity) 
{
    caches ::LRUCache<int> cache(0);

    int local_counter_callback = 0;
    cache.insert (7, 123, CallBack, &local_counter_callback);
    EXPECT_TRUE (cache.empty());
    EXPECT_EQ (local_counter_callback, 1u);
}

struct EvictionLog
{
    int calls = 0;
    int last_key = 0;
    int last_page = 0;
};

void RecordEviction(int key, int page, EvictionLog* log)
{
    ++log->calls;
    log->last_key = key;
    log->last_page = page;
}

int LoadTestPage(int key)
{
    return key * 100;
}

TEST(LRUCacheTest, HitUpdatesEvictionOrder)
{
    caches::LRUCache<int> cache(2);
    EvictionLog log;

    EXPECT_FALSE(cache.lookup_update(1, LoadTestPage, RecordEviction, &log));
    EXPECT_FALSE(cache.lookup_update(2, LoadTestPage, RecordEviction, &log));
    EXPECT_TRUE(cache.lookup_update(1, LoadTestPage, RecordEviction, &log));
    EXPECT_FALSE(cache.lookup_update(3, LoadTestPage, RecordEviction, &log));

    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(log.calls, 1);
    EXPECT_EQ(log.last_key, 2);
    EXPECT_EQ(log.last_page, 200);
    EXPECT_FALSE(cache.extract(2).has_value());
    EXPECT_EQ(cache.size(), 2u);

    auto first = cache.extract(1);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, 100);
    auto third = cache.extract(3);
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(*third, 300);
    EXPECT_TRUE(cache.empty());
}

TEST(LRUCacheTest, MissingExtractionPreservesEvictionOrder)
{
    caches::LRUCache<int> cache(2);
    EvictionLog log;
    cache.insert(1, 100, RecordEviction, &log);
    cache.insert(2, 200, RecordEviction, &log);

    auto missing = cache.extract(99);
    EXPECT_FALSE(missing.has_value());
    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(log.calls, 0);

    cache.insert(3, 300, RecordEviction, &log);
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
