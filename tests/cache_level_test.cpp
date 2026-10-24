#include "cache_level.hpp"
#include <gtest/gtest.h>
#include <string>

struct CacheLevelKeyLog {
    int calls = 0;
    std::string last_key;
    int last_page = 0;
};

void RecordCacheLevelKey(std::string key, int page, CacheLevelKeyLog* log) {
    ++log->calls;
    log->last_key = key;
    log->last_page = page;
}

struct CacheLevelPageLog {
    int calls = 0;
    int last_key = 0;
    std::string last_page;
};

void RecordCacheLevelPage(int key, std::string page, CacheLevelPageLog* log) {
    ++log->calls;
    log->last_key = key;
    log->last_page = page;
}

TEST(CacheLevelTest, RoutesLRUWithStringKeys) {
    caches::CacheLevel<int, std::string> level(caches::Policy::LRU, 2);
    CacheLevelKeyLog log;

    EXPECT_EQ(level.capacity(), 2u);
    EXPECT_FALSE(level.touch("missing"));
    level.insert("alpha", 11, RecordCacheLevelKey, &log);
    level.insert("beta", 22, RecordCacheLevelKey, &log);
    EXPECT_TRUE(level.touch("alpha"));
    level.insert("gamma", 33, RecordCacheLevelKey, &log);

    EXPECT_EQ(log.calls, 1);
    EXPECT_EQ(log.last_key, "beta");
    EXPECT_EQ(log.last_page, 22);
    EXPECT_FALSE(level.extract("beta").has_value());

    auto alpha = level.extract("alpha");
    ASSERT_TRUE(alpha.has_value());
    EXPECT_EQ(*alpha, 11);
    auto gamma = level.extract("gamma");
    ASSERT_TRUE(gamma.has_value());
    EXPECT_EQ(*gamma, 33);
    EXPECT_FALSE(level.touch("alpha"));
    EXPECT_FALSE(level.touch("gamma"));
}

TEST(CacheLevelTest, RoutesLFUWithStringKeys) {
    caches::CacheLevel<int, std::string> level(caches::Policy::LFU, 2);
    CacheLevelKeyLog log;

    EXPECT_FALSE(level.touch("missing"));
    level.insert("alpha", 11, RecordCacheLevelKey, &log);
    EXPECT_TRUE(level.touch("alpha"));
    level.insert("beta", 22, RecordCacheLevelKey, &log);
    level.insert("gamma", 33, RecordCacheLevelKey, &log);

    EXPECT_EQ(log.calls, 1);
    EXPECT_EQ(log.last_key, "beta");
    EXPECT_EQ(log.last_page, 22);
    EXPECT_FALSE(level.extract("beta").has_value());

    auto alpha = level.extract("alpha");
    ASSERT_TRUE(alpha.has_value());
    EXPECT_EQ(*alpha, 11);
    auto gamma = level.extract("gamma");
    ASSERT_TRUE(gamma.has_value());
    EXPECT_EQ(*gamma, 33);
}

TEST(CacheLevelTest, StoresStringPagesWithIntegerKeys) {
    for (auto policy : {caches::Policy::LRU, caches::Policy::LFU}) {
        caches::CacheLevel<std::string> level(policy, 1);
        CacheLevelPageLog log;

        level.insert(7, "original", RecordCacheLevelPage, &log);
        EXPECT_TRUE(level.touch(7));
        level.insert(8, "new", RecordCacheLevelPage, &log);

        EXPECT_EQ(log.calls, 1);
        EXPECT_EQ(log.last_key, 7);
        EXPECT_EQ(log.last_page, "original");
        EXPECT_FALSE(level.extract(7).has_value());

        auto page = level.extract(8);
        ASSERT_TRUE(page.has_value());
        EXPECT_EQ(*page, "new");
        EXPECT_FALSE(level.touch(8));
    }
}

TEST(CacheLevelTest, ForwardsPagesAtZeroCapacity) {
    for (auto policy : {caches::Policy::LRU, caches::Policy::LFU}) {
        caches::CacheLevel<int, std::string> level(policy, 0);
        CacheLevelKeyLog log;

        level.insert("alpha", 123, RecordCacheLevelKey, &log);

        EXPECT_EQ(level.capacity(), 0u);
        EXPECT_EQ(log.calls, 1);
        EXPECT_EQ(log.last_key, "alpha");
        EXPECT_EQ(log.last_page, 123);
        EXPECT_FALSE(level.touch("alpha"));
        EXPECT_FALSE(level.extract("alpha").has_value());
    }
}

TEST(CacheLevelTest, RejectsUnsupportedPolicies) {
    for (auto policy : {caches::Policy::ARC, caches::Policy::TWO_Q, caches::Policy::LIRS}) {
        EXPECT_THROW(caches::CacheLevel<int>(policy, 2), std::invalid_argument);
    }
}
