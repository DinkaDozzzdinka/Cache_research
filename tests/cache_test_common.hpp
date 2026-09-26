#pragma once

#include "cache_types.hpp"

#include <gtest/gtest.h>
#include <cstddef>
#include <vector>

namespace cache_tests 
{
    struct TestPage {
        caches::Key id;
        int generation;
    };

    struct TestPageLoader {
            int load_count = 0;

            TestPage operator()(const caches::Key& key) {
                ++load_count;
                return TestPage{key, load_count};
            }
    };

    template <typename Cache>
    void check_requests(Cache& cache, const std::vector<caches::Key>& requests,
                        const std::vector<bool>& expected_hits) {
        ASSERT_EQ(requests.size(), expected_hits.size());
        ASSERT_EQ(cache.size(), 0u) << "This helper expects a fresh cache";

        TestPageLoader source;

        for (std::size_t position = 0; position < requests.size(); ++position) {
            const auto key = requests[position];

            SCOPED_TRACE(::testing::Message() << "position=" << position << ", key=" << key);

            const int loads_before_request = source.load_count;
            const auto result = cache.lookup_update(key, source);

            EXPECT_EQ(result.hit, expected_hits[position]);
            EXPECT_EQ(result.value.id, key);
            EXPECT_LE(cache.size(), cache.capacity());

            const int expected_source_loads = loads_before_request + (expected_hits[position] ? 0 : 1);

            EXPECT_EQ(source.load_count, expected_source_loads);
        }
    }

    template <typename Cache>
    void check_repeated_requests(Cache& cache, const caches::Key& key) {
        ASSERT_EQ(cache.size(), 0u) << "Expected an empty cache";
        ASSERT_GT(cache.capacity(), 0u);

        TestPageLoader source;

        const auto first_result = cache.lookup_update(key, source);

        EXPECT_FALSE(first_result.hit);
        EXPECT_EQ(first_result.value.id, key);
        EXPECT_EQ(first_result.value.generation, 1);

        for (int repeat = 0; repeat < 3; ++repeat) {
            SCOPED_TRACE(::testing::Message() << "repeat=" << repeat);

            const auto result = cache.lookup_update(7, source);

            EXPECT_TRUE(result.hit);
            EXPECT_EQ(result.value.id, key);
            EXPECT_EQ(result.value.generation, 1);
        }
        
        EXPECT_EQ(source.load_count, 1);
        EXPECT_EQ(cache.size(), 1u);
    }
      
} 