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
    class LirsCache {
    private:
        enum class Status {
            LIR,
            RESIDENT_HIR,
            NON_RESIDENT_HIR
        };

        struct Entry {
            KeyT key;
            T page;
            Status status;
        };

        using StackList = std::list;
        using StackIterator = typename StackList::iterator;
        using QueueList = std::list;
        using QueueIterator = typename QueueList::iterator;

        std::size_t capacity_;
        std::size_t lir_limit_;
        std::size_t hir_limit_;

        std::size_t lir_count_ = 0;

        StackList stack_s_;
        QueueList queue_q_;

        std::unordered_map table_;
        std::unordered_map stack_index_;
        std::unordered_map queue_index_;

        void prune_stack() {
            while (!stack_s_.empty()) {
                const KeyT bottom_key = stack_s_.back();
                auto found = table_.find(bottom_key);
                if (found != table_.end() && found->second.status != Status::LIR) {
                    stack_index_.erase(bottom_key);
                    stack_s_.pop_back();
                } else {
                    break;
                }
            }
        }

        void add_to_stack(const KeyT& key) {
            stack_s_.push_front(key);
            try {
                stack_index_.emplace(key, stack_s_.begin());
            } catch (...) {
                stack_s_.pop_front();
                throw;
            }
        }

        void add_to_queue(const KeyT& key) {
            queue_q_.push_front(key);
            try {
                queue_index_.emplace(key, queue_q_.begin());
            } catch (...) {
                queue_q_.pop_front();
                throw;
            }
        }

        void remove_from_stack(const KeyT& key) {
            auto found = stack_index_.find(key);
            if (found == stack_index_.end()) {
                return;
            }
            stack_s_.erase(found->second);
            stack_index_.erase(found);
        }

        void remove_from_queue(const KeyT& key) {
            auto found = queue_index_.find(key);
            if (found == queue_index_.end()) {
                return;
            }
            queue_q_.erase(found->second);
            queue_index_.erase(found);
        }

        void evict_from_hir() {
            if (queue_q_.empty()) {
                return;
            }

            const KeyT victim_key = queue_q_.back();
            remove_from_queue(victim_key);

            auto found = table_.find(victim_key);
            if (found != table_.end()) {
                if (stack_index_.find(victim_key) != stack_index_.end()) {
                    found->second.status = Status::NON_RESIDENT_HIR;
                    found->second.page = T{};
                } else {
                    table_.erase(found);
                }
            }
        }

        void demote_bottom_lir_to_hir() {
            prune_stack();
            if (stack_s_.empty()) {
                return;
            }

            const KeyT demoted_key = stack_s_.back();
            stack_index_.erase(demoted_key);
            stack_s_.pop_back();

            table_[demoted_key].status = Status::RESIDENT_HIR;
            add_to_queue(demoted_key);
            --lir_count_;

            prune_stack();
        }

        void free_one_place() {
            if (queue_q_.size() >= hir_limit_ && hir_limit_ > 0) {
                evict_from_hir();
            }
        }

    public:
        explicit LirsCache(std::size_t capacity)
            : capacity_(capacity),
              lir_limit_((capacity <= 2) ? 1 : static_cast(capacity * 0.9)),
              hir_limit_(capacity - lir_limit_) {}

        LirsCache(const LirsCache&) = delete;
        LirsCache& operator=(const LirsCache&) = delete;

        std::size_t size() const noexcept {
            return lir_count_ + queue_q_.size();
        }

        std::size_t capacity() const noexcept {
            return capacity_;
        }

        bool empty() const noexcept {
            return size() == 0;
        }

        template 
        LookupResult lookup_update(
            const Key& key,
            Loader&& load
        ) {
            auto found = table_.find(key);

            if (found != table_.end() && found->second.status == Status::LIR) {
                T page = found->second.page;

                auto s_it = stack_index_.find(key);
                if (s_it != stack_index_.end()) {
                    stack_s_.splice(stack_s_.begin(), stack_s_, s_it->second);
                    prune_stack();
                }

                return {std::move(page), true};
            }

            if (found != table_.end() && found->second.status == Status::RESIDENT_HIR) {
                T page = found->second.page;
                auto s_it = stack_index_.find(key);

                if (s_it != stack_index_.end()) {
                    found->second.status = Status::LIR;
                    ++lir_count_;
                    remove_from_queue(key);

                    stack_s_.splice(stack_s_.begin(), stack_s_, s_it->second);

                    if (lir_count_ > lir_limit_) {
                        demote_bottom_lir_to_hir();
                    }
                } else {
                    remove_from_queue(key);
                    add_to_queue(key);
                    add_to_stack(key);
                }

                return {std::move(page), true};
            }

            const bool is_ghost = (found != table_.end() && found->second.status == Status::NON_RESIDENT_HIR);
            T page = load(key);

            if (capacity_ == 0) {
                return {std::move(page), false};
            }

            if (lir_count_ < lir_limit_ && !is_ghost) {
                table_.emplace(key, Entry{key, page, Status::LIR});
                add_to_stack(key);
                ++lir_count_;
                return {std::move(page), false};
            }

            free_one_place();

            if (is_ghost) {
                found->second.page = page;
                found->second.status = Status::LIR;
                ++lir_count_;

                auto s_it = stack_index_.find(key);
                if (s_it != stack_index_.end()) {
                    stack_s_.splice(stack_s_.begin(), stack_s_, s_it->second);
                } else {
                    add_to_stack(key);
                }

                if (lir_count_ > lir_limit_) {
                    demote_bottom_lir_to_hir();
                }
            } else {
                table_.insert_or_assign(key, Entry{key, page, Status::RESIDENT_HIR});
                add_to_queue(key);
                add_to_stack(key);
            }

            return {std::move(page), false};
        }
    };

} 