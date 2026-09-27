// 1-level lru_example from lecture

#pragma once

#include "cache_types.hpp"

#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>

namespace caches 
{
    template <typename T, typename KeyT = Key>          // T - page type
    class LruCache {
    private:
        using Entry = std::pair<KeyT, T>;
        using List = std::list<Entry>;
        using ListIterator = typename List::iterator;   // dependent names

        std::size_t capacity_;
        List pages_;
        std::unordered_map<KeyT, ListIterator> index_;

        void move_to_front(ListIterator position) {
            pages_.splice(pages_.begin(), pages_, position);
        }

        void throw_oldest() {                            // not empty list is obligated
            const KeyT& key = pages_.back().first;

            index_.erase(key);
            pages_.pop_back();
        }

        void insert(const KeyT& key, const T& page) {
            pages_.emplace_front(key, page);

            try {
                index_.emplace(key, pages_.begin());
            } catch (...) {
                pages_.pop_front();
                throw;
            }

            if (pages_.size() > capacity_) {
                throw_oldest();
            }
        }

    public:
        explicit LruCache(std::size_t capacity)
            : capacity_(capacity) {}

        LruCache(const LruCache&) = delete;             // copying is forbidden
        LruCache& operator=(const LruCache&) = delete;


        std::size_t size() const noexcept {
            return pages_.size();
        }

        std::size_t capacity() const noexcept {
            return capacity_;
        }

        bool empty() const noexcept {
            return pages_.empty();
        }

        template <typename Loader>
        LookupResult<T> lookup_update(const KeyT& key, Loader&& load_page) {
            auto found = index_.find(key);

            if (found != index_.end()) {
                auto list_it = found->second;
                T page = list_it->second;
                move_to_front(found->second);

                return {std::move(page), true};
            }

            T page = load_page(key);

            if (capacity_ != 0) {
                insert(key, page);
            }

            return {std::move(page), false};
        }
    };
} 