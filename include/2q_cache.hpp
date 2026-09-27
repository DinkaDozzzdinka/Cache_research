#pragma once

#include "cache_types.hpp"

#include <algorithm>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>

namespace caches {

    template <typename T, typename KeyT = Key>
    class TwoQCache {
    private:
        using Entry = std::pair<KeyT, T>;
        using PageList = std::list<Entry>;
        using PageIterator = typename PageList::iterator;
        using GhostList = std::list<KeyT>;
        using GhostIterator = typename GhostList::iterator;
        
        PageList a1in_;   // FIFO: new pages here
        PageList am_;     // LRU: repeated request
        GhostList a1out_; // FIFO: page's keys evicted from a1in 
        
        std::size_t capacity_;
        std::size_t a1in_limit_;
        std::size_t a1out_limit_;

        std::unordered_map<KeyT, PageIterator> a1in_index_;
        std::unordered_map<KeyT, PageIterator> am_index_;
        std::unordered_map<KeyT, GhostIterator> a1out_index_;

        
        void add_to_a1in(const KeyT& key, const T& page) {
            a1in_.emplace_front(key, page);

            try {
                a1in_index_.emplace(key, a1in_.begin());
            } catch (...) {
                a1in_.pop_front();
                throw;
            }
        }

        void add_to_am(const KeyT& key, const T& page) {
            am_.emplace_front(key, page);

            try {
                am_index_.emplace(key, am_.begin());
            } catch (...) {
                am_.pop_front();
                throw;
            }
        }

        // remember key from ghost evicted from a1in
        void add_to_a1out(const KeyT& key) {
            if (a1out_limit_ == 0) {
                return;
            }

            a1out_.push_front(key);

            try {
                a1out_index_.emplace(key, a1out_.begin());
            } catch (...) {
                a1out_.pop_front();
                throw;
            }

            if (a1out_.size() > a1out_limit_) {
                evict_oldest_from_a1out();
            }
        }

        void evict_oldest_from_a1out() {
            a1out_index_.erase(a1out_.back());
            a1out_.pop_back();
        }

        // when key is loaded in Am, so there is no use in storing it in A1out
        void remove_from_a1out(const KeyT& key) {
            auto found = a1out_index_.find(key);
            if (found == a1out_index_.end()) {
                return;
            }

            a1out_.erase(found->second);
            a1out_index_.erase(found);
        }

        void evict_from_a1in() {
            const KeyT& key = a1in_.back().first;

            // remember key, then delete page
            add_to_a1out(key);

            a1in_index_.erase(key);
            a1in_.pop_back();
        }

        void evict_from_am() {
            am_index_.erase(am_.back().first);
            am_.pop_back();
        }

        void free_one_place() {
            if (size() < capacity_) {
                return;
            }

            if (!a1in_.empty() && (a1in_.size() >= a1in_limit_ || am_.empty())) {
                evict_from_a1in();
            } else {
                evict_from_am();
            }
        }

    public:
        explicit TwoQCache(std::size_t capacity)
            : capacity_(capacity),
            a1in_limit_((capacity == 0) ? 0 : std::max<std::size_t>(1, capacity / 4)),
            a1out_limit_(capacity / 2) {}

        TwoQCache(const TwoQCache&) = delete;
        TwoQCache& operator=(const TwoQCache&) = delete;

        std::size_t size() const noexcept {
            return a1in_.size() + am_.size();
        }

        std::size_t capacity() const noexcept {
            return capacity_;
        }

        
        template <typename Loader>
        LookupResult<T> lookup_update(const KeyT& key, Loader&& load_page) {

            // hit in Am
            if (auto found = am_index_.find(key); 
                found != am_index_.end()) 
            {
                PageIterator page_it = found->second;
                T page = page_it->second;
                am_.splice(am_.begin(), am_, page_it);
                return {std::move(page), true};
            }

            // hit in A1in
            if (auto found = a1in_index_.find(key);
                found != a1in_index_.end()) 
            {
                PageIterator page_it = found->second;
                T page = page_it->second;
;

                am_index_.emplace(key, page_it);
                a1in_index_.erase(found);
                am_.splice(am_.begin(), a1in_, page_it);

                return {std::move(page), true};
            }

            const bool ghost_hit = a1out_index_.find(key) != a1out_index_.end();
            T page = load_page(key);

            if (capacity_ == 0) {
                return {std::move(page), false};
            }

            free_one_place();

            if (ghost_hit) {
                add_to_am(key, page);
                remove_from_a1out(key);
            } else {
                add_to_a1in(key, page);
            }

            return {std::move(page), false};
        }
    };
} 