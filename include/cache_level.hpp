#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <variant>

#include "config.hpp"
#include "lfu.hpp"
#include "lru.hpp"

namespace caches
{
    template <typename T, typename KeyT = int>
    class CacheLevel
    {
        public:
            explicit CacheLevel(Policy policy, std::size_t capacity): capacity_(capacity)
            {
                if (policy == Policy::LRU)
                    cache_.template emplace<LRUCache<T, KeyT>>(capacity);
                else if (policy == Policy::LFU)
                    cache_.template emplace<LFUCache<T, KeyT>>(capacity);
                else
                    throw std::invalid_argument("Only LRU and LFU are implemented");
            }

            bool touch(KeyT key)
            {
                auto* lru = std::get_if<LRUCache<T, KeyT>>(&cache_);
                if (lru != nullptr)
                    return lru->touch(key);
                auto* lfu = std::get_if<LFUCache<T, KeyT>>(&cache_);
                if (lfu != nullptr)
                    return lfu->touch(key);
                throw std::logic_error("Cache level has no active policy");
            }

            std::optional<T> extract(KeyT key)
            {
                auto* lru = std::get_if<LRUCache<T, KeyT>>(&cache_);
                if (lru != nullptr)
                    return lru->extract(key);
                auto* lfu = std::get_if<LFUCache<T, KeyT>>(&cache_);
                if (lfu != nullptr)
                    return lfu->extract(key);
                throw std::logic_error("Cache level has no active policy");
            }

            template <typename H, typename G>
            void insert(KeyT key, T page, H CallBack, G TransferContext)
            {
                auto* lru = std::get_if<LRUCache<T, KeyT>>(&cache_);
                if (lru != nullptr)
                {
                    lru->insert(key, page, CallBack, TransferContext);
                    return;
                }
                auto* lfu = std::get_if<LFUCache<T, KeyT>>(&cache_);
                if (lfu != nullptr)
                {
                    lfu->insert(key, page, CallBack, TransferContext);
                    return;
                }
                throw std::logic_error("Cache level has no active policy");
            }

            std::size_t capacity() const noexcept
            {
                return capacity_;
            }

        private:
            std::variant<std::monostate, LRUCache<T, KeyT>, LFUCache<T, KeyT>> cache_;
            std::size_t capacity_;
    };
}
