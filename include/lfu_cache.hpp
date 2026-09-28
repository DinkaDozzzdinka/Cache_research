#pragma once

#include "cache_types.hpp"

#include <algorithm>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>

namespace caches 
{

    template <typename T, typename KeyT = Key>
    class LfuCache {
    private:
        struct Node {
            KeyT key;
            T page;
            std::size_t freq;
        };

        using NodeList = std::list;
        using NodeIterator = typename NodeList::iterator;

        std::size_t capacity_;
        std::size_t min_freq_ = 0;

        std::unordered_map freq_buckets_;
        std::unordered_map index_;

        void increase_frequency(NodeIterator node_it) {
            const std::size_t old_freq = node_it->freq;
            const std::size_t new_freq = old_freq + 1;
            node_it->freq = new_freq;

            freq_buckets_[new_freq].splice(
                freq_buckets_[new_freq].begin(),
                freq_buckets_[old_freq],
                node_it
            );

            if (freq_buckets_[old_freq].empty()) {
                freq_buckets_.erase(old_freq);
                if (min_freq_ == old_freq) {
                    min_freq_ = new_freq;
                }
            }
        }

        void add_node(const KeyT& key, const T& page) {
            min_freq_ = 1;
            auto& bucket = freq_buckets_[min_freq_];
            bucket.emplace_front(Node{key, page, 1});

            try {
                index_.emplace(key, bucket.begin());
            } catch (...) {
                bucket.pop_front();
                if (bucket.empty()) {
                    freq_buckets_.erase(min_freq_);
                }
                throw;
            }
        }

        void evict_least_frequent() {
            auto bucket_it = freq_buckets_.find(min_freq_);
            if (bucket_it == freq_buckets_.end() || bucket_it->second.empty()) {
                return;
            }

            auto& bucket = bucket_it->second;
            const KeyT victim_key = bucket.back().key;

            index_.erase(victim_key);
            bucket.pop_back();

            if (bucket.empty()) {
                freq_buckets_.erase(bucket_it);
            }
        }

        void free_one_place() {
            if (size() < capacity_) {
                return;
            }
            evict_least_frequent();
        }

    public:
        explicit LfuCache(std::size_t capacity)
            : capacity_(capacity) {}

        LfuCache(const LfuCache&) = delete;
        LfuCache& operator=(const LfuCache&) = delete;

        std::size_t size() const noexcept {
            return index_.size();
        }

        std::size_t capacity() const noexcept {
            return capacity_;
        }

        bool empty() const noexcept {
            return index_.empty();
        }

        template 
        LookupResult lookup_update(
            const Key& key,
            Loader&& load
        ) {
            if (auto found = index_.find(key); found != index_.end()) {
                NodeIterator node_it = found->second;
                T page = node_it->page;
                increase_frequency(node_it);
                return {std::move(page), true};
            }

            T page = load(key);

            if (capacity_ == 0) {
                return {std::move(page), false};
            }

            free_one_place();
            add_node(key, page);

            return {std::move(page), false};
        }
    };

} 