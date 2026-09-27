#pragma once

#include "cache_types.hpp"

#include <algorithm>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>

namespace caches {

template <typename T, typename KeyT = Key>
class ArcCache {
private:
    struct Entry {
        KeyT key;
        T value;
    };

    using PageList = std::list<Entry>;
    using PageIterator = typename PageList::iterator;
    using PageIndex = std::unordered_map<KeyT, PageIterator>;

    using GhostList = std::list<KeyT>;
    using GhostIterator = typename GhostList::iterator;
    using GhostIndex = std::unordered_map<KeyT, GhostIterator>;

    std::size_t capacity_;
    std::size_t p_ = 0;

    PageList t1_;
    PageList t2_;

    PageIndex t1_index_;
    PageIndex t2_index_;

    GhostList b1_;
    GhostList b2_;

    GhostIndex b1_index_;
    GhostIndex b2_index_;


    void add_page(PageList& pages, PageIndex& index, const KeyT& key, const T& page) 
    {
        pages.push_front(Entry{key, page});

        try {
            index.emplace(key, pages.begin());
        } catch (...) {
            pages.pop_front();
            throw;
        }
    }

    void add_ghost(GhostList& ghosts, GhostIndex& index, const KeyT& key) 
    {
        ghosts.push_front(key);

        try {
            index.emplace(key, ghosts.begin());
        } catch (...) {
            ghosts.pop_front();
            throw;
        }
    }

    void evict_oldest_page(PageList& pages, PageIndex& index) {
        index.erase(pages.back().key);
        pages.pop_back();
    }

    void evict_oldest_ghost(GhostList& ghosts, GhostIndex& index) {
        index.erase(ghosts.back());
        ghosts.pop_back();
    }

    void remove_ghost(GhostList& ghosts, GhostIndex& index, const KeyT& key) 
    {
        auto found = index.find(key);

        if (found == index.end()) {
            return;
        }

        ghosts.erase(found->second);
        index.erase(found);
    }

    void evict_to_ghost(PageList& pages, PageIndex& page_index, 
                        GhostList& ghosts, GhostIndex& ghost_index) {
        const KeyT& key = pages.back().key;

        add_ghost(ghosts, ghost_index, key);
        evict_oldest_page(pages, page_index);
    }

    void move_to_t2(PageIterator page_it) {
        t2_index_.emplace(page_it->key, page_it);
        t1_index_.erase(page_it->key);
        t2_.splice(t2_.begin(), t1_, page_it);
    }

    void increase_p() {
        // hit in B1
        const std::size_t delta = std::max<std::size_t>(1, b2_.size() / b1_.size());
        p_ += std::min(capacity_ - p_, delta);
    }

    void decrease_p() {
        // hit in B2
        const std::size_t delta = std::max<std::size_t>(1, b1_.size() / b2_.size());
        p_ -= std::min(p_, delta);
    }

    void replace(bool hit_in_b2) {
        if (size() < capacity_)     { return;}

        const bool evict_t1 = !t1_.empty() &&
                              (t1_.size() > p_ || (hit_in_b2 && t1_.size() == p_));

        if (evict_t1) {
            evict_to_ghost(t1_, t1_index_, b1_, b1_index_);
        } else {
            evict_to_ghost(t2_, t2_index_, b2_, b2_index_);
        }
    }

    // the key is not in any of the four lists, before adding to T1
    void prepare_place_for_new_key() {
        const std::size_t recent_size = t1_.size() + b1_.size();

        if (recent_size == capacity_) {
            if (t1_.size() < capacity_) {
                // B1 не пуст: освобождаем место в истории T1.
                evict_oldest_ghost(b1_, b1_index_);

                replace(false);
            } else {
                // Весь кэш занят T1.
                // Здесь страницу удаляем без сохранения в B1.
                evict_oldest_page(t1_, t1_index_);
            }

            return;
        }

        const std::size_t total_size = size() + b1_.size() + b2_.size();

        if (total_size >= capacity_) {
            // Эквивалент total_size == 2 * capacity_,
            // но без умножения ёмкости.
            if (total_size - capacity_ == capacity_) {
                evict_oldest_ghost(b2_, b2_index_);
            }

            replace(false);
        }
    }


public:
    explicit ArcCache(std::size_t capacity)
        : capacity_(capacity) {}

    ArcCache(const ArcCache&) = delete;
    ArcCache& operator=(const ArcCache&) = delete;

    std::size_t size() const noexcept {
        return t1_.size() + t2_.size();
    }

    std::size_t capacity() const noexcept {
        return capacity_;
    }

    bool empty() const noexcept {
        return size() == 0;
    }

    std::size_t target_t1_size() const noexcept {
        return p_;
    }


    template <typename Loader>
    LookupResult<T> lookup_update(const KeyT& key, Loader&& load_page) {
        // hit in T1
        auto found_t1 = t1_index_.find(key);

        if (found_t1 != t1_index_.end()) {
            PageIterator page_it = found_t1->second;
            T page = page_it->value;

            move_to_t2(page_it);

            return {std::move(page), true};
        }

        // hit in T2
        auto found_t2 = t2_index_.find(key);

        if (found_t2 != t2_index_.end()) {
            PageIterator page_it = found_t2->second;
            T page = page_it->value;

            t2_.splice(t2_.begin(), t2_, page_it);

            return {std::move(page), true};
        }

        const bool in_b1 = b1_index_.find(key) != b1_index_.end();
        const bool in_b2 = b2_index_.find(key) != b2_index_.end();

        T page = load_page(key);

        if (capacity_ == 0) {
            return {std::move(page), false};
        }

        // hit in B1
        if (in_b1) {
            increase_p();
            replace(false);

            add_page(t2_, t2_index_, key, page);
            remove_ghost(b1_, b1_index_, key);

            return {std::move(page), false};
        }

        // 4. Попадание в B2: увеличиваем предпочтение T2.
        if (in_b2) {
            decrease_p();
            replace(true);

            add_page(t2_, t2_index_, key, page);
            remove_ghost(b2_, b2_index_, key);

            return {std::move(page), false};
        }

        // 5. Совершенно новый ключ.
        prepare_place_for_new_key();
        add_page(t1_, t1_index_, key, page);

        return {std::move(page), false};
    }
};

} 