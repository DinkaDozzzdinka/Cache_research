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

