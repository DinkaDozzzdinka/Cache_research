#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <variant>

#include "config.hpp"
#include "arc.hpp"
#include "lfu.hpp"
#include "lirs.hpp"
#include "lru.hpp"
#include "two_q.hpp"

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
                else if (policy == Policy::TWO_Q)
                    cache_.template emplace<TwoQCache<T, KeyT>>(capacity);
                else if (policy == Policy::ARC)
                    cache_.template emplace<ARCCache<T, KeyT>>(capacity);
                else if (policy == Policy::LIRS)
                    cache_.template emplace<LIRSCache<T, KeyT>>(capacity);
                else
                    throw std::invalid_argument("Unknown cache policy");
            }

            bool touch(KeyT key)
            {
                auto* lru = std::get_if<LRUCache<T, KeyT>>(&cache_);
                if (lru != nullptr)
                    return lru->touch(key);
                auto* lfu = std::get_if<LFUCache<T, KeyT>>(&cache_);
                if (lfu != nullptr)
                    return lfu->touch(key);
                auto* two_q = std::get_if<TwoQCache<T, KeyT>>(&cache_);
                if (two_q != nullptr)
                    return two_q->touch(key);
                auto* arc = std::get_if<ARCCache<T, KeyT>>(&cache_);
                if (arc != nullptr)
                    return arc->touch(key);
                auto* lirs = std::get_if<LIRSCache<T, KeyT>>(&cache_);
                if (lirs != nullptr)
                    return lirs->touch(key);
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
                auto* two_q = std::get_if<TwoQCache<T, KeyT>>(&cache_);
                if (two_q != nullptr)
                    return two_q->extract(key);
                auto* arc = std::get_if<ARCCache<T, KeyT>>(&cache_);
                if (arc != nullptr)
                    return arc->extract(key);
                auto* lirs = std::get_if<LIRSCache<T, KeyT>>(&cache_);
                if (lirs != nullptr)
                    return lirs->extract(key);
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
                auto* two_q = std::get_if<TwoQCache<T, KeyT>>(&cache_);
                if (two_q != nullptr)
                {
                    two_q->insert(key, page, CallBack, TransferContext);
                    return;
                }
                auto* arc = std::get_if<ARCCache<T, KeyT>>(&cache_);
                if (arc != nullptr)
                {
                    arc->insert(key, page, CallBack, TransferContext);
                    return;
                }
                auto* lirs = std::get_if<LIRSCache<T, KeyT>>(&cache_);
                if (lirs != nullptr)
                {
                    lirs->insert(key, page, CallBack, TransferContext);
                    return;
                }
                throw std::logic_error("Cache level has no active policy");
            }

            std::size_t capacity() const noexcept
            {
                return capacity_;
            }

            std:: size_t size() const 
            {
                auto* lru = std::get_if<LRUCache<T, KeyT>>(&cache_);
                if (lru != nullptr)
                    return lru->size();
                auto* lfu = std::get_if<LFUCache<T, KeyT>>(&cache_);
                if (lfu != nullptr)
                    return lfu->size();
                auto* two_q = std::get_if<TwoQCache<T, KeyT>>(&cache_);
                if (two_q != nullptr)
                    return two_q->size();
                auto* arc = std::get_if<ARCCache<T, KeyT>>(&cache_);
                if (arc != nullptr)
                    return arc->size();
                auto* lirs = std::get_if<LIRSCache<T, KeyT>>(&cache_);
                if (lirs != nullptr)
                    return lirs->size();
                throw std::logic_error("Cache level has no active size");
            }

            bool empty() const 
            {
                return size() == 0;
            }

            bool full() const 
            {
                return size() >= capacity_;
            }

        private:
            std::variant<std::monostate, LRUCache<T, KeyT>, LFUCache<T, KeyT>,
                         TwoQCache<T, KeyT>, ARCCache<T, KeyT>, LIRSCache<T, KeyT>> cache_;
            std::size_t capacity_;
    };
}
