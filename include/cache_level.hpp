#pragma once

#include "config.hpp"
#include "cache_types.hpp"
#include "arc_cache.hpp"
#include "lru_cache.hpp"
#include "2q_cache.hpp"
#include "lfu_cache.hpp"
#include "lirs_cache.hpp"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <variant>

namespace caches
{

    template <typename T>
    using LruPointer = std::unique_ptr<LruCache<T>>;

    template <typename T>
    using ArcPointer = std::unique_ptr<ArcCache<T>>;

    template <typename T>
    using TwoQPointer = std::unique_ptr<TwoQCache<T>>;

    template <typename T>
    using LfuPointer = std::unique_ptr<LfuCache<T>>;

    template <typename T>
    using LirsPointer = std::unique_ptr<LirsCache<T>>;

    template <typename T>
    using CacheLevel = std::variant<
        LruPointer,
        ArcPointer,
        TwoQPointer,
        LfuPointer,
        LirsPointer
    >;

    template <typename T>
    CacheLevel<T> make_level(Policy policy, std::size_t capacity)
    {
        switch (policy)
        {
        case Policy::LRU:
            return std::make_unique<LruCache<T>>(capacity);
        case Policy::ARC:
            return std::make_unique<ArcCache<T>>(capacity);
        case Policy::TWO_Q:
            return std::make_unique<TwoQCache<T>>(capacity);
        case Policy::LFU:
            return std::make_unique<LfuCache<T>>(capacity);
        case Policy::LIRS:
            return std::make_unique<LirsCache<T>>(capacity);
        default:
            throw std::invalid_argument(
                "Requested cache policy is not implemented yet");
        }
    }

    template <typename T, typename Loader>
    LookupResult<T> access_level(
        CacheLevel<T>& level,
        const Key& key,
        Loader& load)
    {
        if (auto* pointer = std::get_if<LruPointer<T>>(&level))
        {
            return (*pointer)->lookup_update(key, load);
        }
        if (auto* pointer = std::get_if<ArcPointer<T>>(&level))
        {
            return (*pointer)->lookup_update(key, load);
        }
        if (auto* pointer = std::get_if<TwoQPointer<T>>(&level))
        {
            return (*pointer)->lookup_update(key, load);
        }
        if (auto* pointer = std::get_if<LfuPointer<T>>(&level))
        {
            return (*pointer)->lookup_update(key, load);
        }
        if (auto* pointer = std::get_if<LirsPointer<Y>>(&level))
        {
            return (*pointer)->lookup_update(key, load);
        }

        throw std::logic_error("Unknown cache level type");
    }

}
