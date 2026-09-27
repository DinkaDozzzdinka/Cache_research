#include "cache_test_common.hpp"

#include "lru_cache.hpp"
#include "2q_cache.hpp"
#include "arc_cache.hpp"
#include "ideal_cache.hpp"
// #include "lirs_cache.hpp"
// #include "lru_cache.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

using cache_tests::TestPage;
using cache_tests::check_requests;
using cache_tests::check_repeated_requests;


// ---------- repeated requests ----------

TEST(LruCacheTest, RepeatedRequestsReturnStoredPage) {
    caches::LruCache<TestPage> cache{2};
    const caches::Key key = 7;

    check_repeated_requests(cache, key);
}

TEST(TwoQCacheTest, RepeatedRequestsReturnStoredPage) {
    caches::TwoQCache<TestPage> cache{2};
    const caches::Key key = 7;

    check_repeated_requests(cache, key);
}

TEST(ArcCacheTest, RepeatedRequestsReturnStoredPage) {
    caches::ArcCache<TestPage> cache{2};
    const caches::Key key = 7;

    check_repeated_requests(cache, key);
}

TEST(IdealCacheTest, RepeatedRequestsReturnStoredPage) {
    const caches::Key key = 7;
    const std::vector<caches::Key> requests(4, key);
    caches::IdealCache<TestPage> cache{2, requests};

    check_repeated_requests(cache, key);
}

// ---------- zero capacity ----------

TEST(LruCacheTest, ZeroCapacityAlwaysMisses) {
    caches::LruCache<TestPage> cache{0};

    check_requests(cache, {1, 1, 2, 1}, {false, false, false, false});
    EXPECT_EQ(cache.size(), 0u);
}

TEST(TwoQCacheTest, ZeroCapacityAlwaysMisses) {
    caches::TwoQCache<TestPage> cache{0};

    check_requests(cache, {1, 1, 2, 1}, {false, false, false, false});
    EXPECT_EQ(cache.size(), 0u);
}

TEST(ArcCacheTest, ZeroCapacityAlwaysMisses) {
    caches::ArcCache<TestPage> cache{0};

    check_requests(cache, {1, 1, 2, 1}, {false, false, false, false});
    EXPECT_EQ(cache.size(), 0u);
}

TEST(IdealCacheTest, ZeroCapacityAlwaysMisses) {
    const std::vector<caches::Key> requests{1, 1, 2, 1};
    caches::IdealCache<TestPage> cache{0, requests};

    check_requests(cache, requests, {false, false, false, false});
    EXPECT_EQ(cache.size(), 0u);
}

// ---------- Вытеснение в LRU ----------

TEST(LruCacheTest, HitUpdatesEvictionOrder) {
    caches::LruCache<TestPage> cache{2};

    check_requests(cache,
                   {1, 2, 1, 3, 1, 2},
                   {false, false, true, false, true, false});
}

// ---------- Упрощённый 2Q ----------

TEST(TwoQCacheTest, RepeatedPageSurvivesNewRequests) {
    caches::TwoQCache<TestPage> cache{2};

    check_requests(cache,
                   {1, 1, 2, 3, 1},
                   {false, true, false, false, true});
}

TEST(TwoQCacheTest, GhostRequestLoadsPageIntoMainQueue) {
    caches::TwoQCache<TestPage> cache{2};

    check_requests(cache,
                   {1, 2, 1, 3, 2, 4, 2},
                   {false, false, true, false, false, false, true});
}

// ---------- Адаптация ARC ----------

TEST(ArcCacheTest, GhostRequestsAdjustTargetSize) {
    caches::ArcCache<TestPage> cache{2};

    int source_calls = 0;
    auto source = [&](const caches::Key& key) {
        return TestPage{key, ++source_calls};
    };

    EXPECT_EQ(cache.target_t1_size(), 0u);

    EXPECT_FALSE(cache.lookup_update(1, source).hit);
    EXPECT_FALSE(cache.lookup_update(2, source).hit);
    EXPECT_TRUE(cache.lookup_update(1, source).hit);
    EXPECT_FALSE(cache.lookup_update(3, source).hit);

    // Теперь ключ 2 находится в B1.
    EXPECT_FALSE(cache.lookup_update(2, source).hit);
    EXPECT_EQ(cache.target_t1_size(), 1u);

    // При предыдущем запросе страница 1 вытеснена из T2 в B2.
    EXPECT_FALSE(cache.lookup_update(1, source).hit);
    EXPECT_EQ(cache.target_t1_size(), 0u);

    EXPECT_EQ(source_calls, 5);
    EXPECT_EQ(cache.size(), 2u);
}

TEST(ArcCacheTest, UniqueRequestsKeepTargetAtZero) {
    caches::ArcCache<TestPage> cache{2};

    check_requests(cache,
                   {1, 2, 3, 4, 5},
                   {false, false, false, false, false});

    EXPECT_EQ(cache.target_t1_size(), 0u);
}

// ---------- Идеальный кэш ----------

TEST(IdealCacheTest, EvictsPageNeededFarthestInFuture) {
    const std::vector<caches::Key> requests{1, 2, 3, 1, 2, 3};
    caches::IdealCache<TestPage> cache{2, requests};

    check_requests(cache,
                   requests,
                   {false, false, false, true, false, true});
}

TEST(IdealCacheTest, RejectsWrongKeyWithoutAdvancingPosition) {
    const std::vector<caches::Key> requests{1};
    caches::IdealCache<TestPage> cache{1, requests};

    int source_calls = 0;
    auto source = [&](const caches::Key& key) {
        return TestPage{key, ++source_calls};
    };

    EXPECT_THROW(cache.lookup_update(2, source), std::invalid_argument);
    EXPECT_EQ(source_calls, 0);
    EXPECT_EQ(cache.size(), 0u);

    // Ошибочный запрос не должен сдвинуть текущую позицию.
    EXPECT_FALSE(cache.lookup_update(1, source).hit);
    EXPECT_EQ(source_calls, 1);

    EXPECT_THROW(cache.lookup_update(1, source), std::out_of_range);
    EXPECT_EQ(source_calls, 1);
}

TEST(IdealCacheTest, EmptySequenceRejectsRequests) {
    const std::vector<caches::Key> requests;
    caches::IdealCache<TestPage> cache{2, requests};

    int source_calls = 0;
    auto source = [&](const caches::Key& key) {
        return TestPage{key, ++source_calls};
    };

    EXPECT_EQ(cache.size(), 0u);
    EXPECT_THROW(cache.lookup_update(1, source), std::out_of_range);
    EXPECT_EQ(source_calls, 0);
}