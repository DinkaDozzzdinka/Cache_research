#pragma once

#include <algorithm>
#include <cstddef>
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace caches
{
    template <typename T, typename KeyT = int>
    class LIRSCache
    {
        public:
            explicit LIRSCache(std::size_t capacity) : capacity_(capacity), lir_capacity_(capacity - (capacity == 0 ? 0 : std::max(std::size_t{1}, capacity / 100))) {}

            LIRSCache(const LIRSCache&) = delete;
            LIRSCache& operator=(const LIRSCache&) = delete;

            template <typename F, typename H, typename G>
            bool lookup_update(KeyT key, F SlowGetPage, H CallBack, G TransferContext)
            {
                if (touch(key))
                    return true;
                T page = SlowGetPage(key);
                insert(key, page, CallBack, TransferContext);
                return false;
            }

            bool touch(KeyT key)
            {
                auto hit = entries_.find(key);
                if (hit == entries_.end() || !hit->second.page)
                    return false;
                auto& entry = hit->second;
                if (entry.lir)
                    stack_.splice(stack_.begin(), stack_, entry.stack_it);
                else if (entry.in_stack && lir_capacity_ != 0)
                    Promote(entry, true);
                else
                {
                    AddToStack(key, entry);
                    queue_.splice(queue_.begin(), queue_, entry.queue_it);
                }
                PruneStack();
                return true;
            }

            template <typename H, typename G>
            void insert(KeyT key, T page, H CallBack, G TransferContext)
            {
                if (capacity_ == 0)
                {
                    CallBack(key, page, TransferContext);
                    return;
                }
                auto known = entries_.find(key);
                if (known != entries_.end() && known->second.page)
                    throw std::invalid_argument("LIRS insert requires an absent page");

                std::optional<std::pair<KeyT, T>> evicted;
                if (full())
                {
                    const auto& victim_key = queue_.back();
                    evicted.emplace(victim_key, *entries_.at(victim_key).page);
                }

                auto data = std::make_unique<T>(page);
                if (known != entries_.end())
                {
                    Promote(known->second, false);
                    known->second.page = std::move(data);
                }
                else
                {
                    const bool lir = lir_count_ < lir_capacity_;
                    AddNew(key, std::move(data), lir);
                    if (lir)
                        ++lir_count_;
                }

                if (evicted)
                    Evict(evicted->first);
                PruneStack();
                if (evicted)
                    CallBack(evicted->first, evicted->second, TransferContext);
            }

            std::optional<T> extract(KeyT key)
            {
                auto hit = entries_.find(key);
                if (hit == entries_.end() || !hit->second.page)
                    return std::nullopt;
                auto& entry = hit->second;
                T page = *entry.page;
                if (entry.lir)
                {
                    stack_.erase(entry.stack_it);
                    --lir_count_;
                }
                else
                {
                    queue_.erase(entry.queue_it);
                    if (entry.in_stack)
                    {
                        stack_.erase(entry.stack_it);
                        hir_stack_.erase(entry.hir_it);
                    }
                }
                entries_.erase(hit);
                PruneStack();
                return page;
            }

            std::size_t size() const noexcept { return lir_count_ + queue_.size(); }
            std::size_t capacity() const noexcept { return capacity_; }
            bool empty() const noexcept { return size() == 0; }
            bool full() const noexcept { return size() >= capacity_; }

        private:
            using KeyList = std::list<KeyT>;
            using KeyIt = typename KeyList::iterator;

            struct Entry
            {
                std::unique_ptr<T> page;
                bool lir;
                bool in_stack;
                KeyIt stack_it;
                KeyIt queue_it;
                KeyIt hir_it;
            };

            void AddNew(const KeyT& key, std::unique_ptr<T> data, bool lir)
            {
                stack_.push_front(key);
                bool added_hir = false;
                bool added_queue = false;
                try
                {
                    if (!lir)
                    {
                        hir_stack_.push_front(key);
                        added_hir = true;
                        queue_.push_front(key);
                        added_queue = true;
                    }
                    entries_.emplace(key, Entry{std::move(data), lir, true, stack_.begin(),
                        lir ? KeyIt{} : queue_.begin(), lir ? KeyIt{} : hir_stack_.begin()});
                }
                catch (...)
                {
                    if (added_queue)
                        queue_.pop_front();
                    if (added_hir)
                        hir_stack_.pop_front();
                    stack_.pop_front();
                    throw;
                }
            }

            void AddToStack(const KeyT& key, Entry& entry)
            {
                if (entry.in_stack)
                {
                    stack_.splice(stack_.begin(), stack_, entry.stack_it);
                    hir_stack_.splice(hir_stack_.begin(), hir_stack_, entry.hir_it);
                    return;
                }
                stack_.push_front(key);
                try
                {
                    hir_stack_.push_front(key);
                }
                catch (...)
                {
                    stack_.pop_front();
                    throw;
                }
                entry.stack_it = stack_.begin();
                entry.hir_it = hir_stack_.begin();
                entry.in_stack = true;
            }

            void Promote(Entry& entry, bool was_resident)
            {
                if (lir_count_ == lir_capacity_)
                {
                    const auto& key = stack_.back();
                    auto& demoted = entries_.at(key);
                    queue_.push_front(key);
                    demoted.queue_it = queue_.begin();
                    demoted.lir = false;
                    demoted.in_stack = false;
                    stack_.pop_back();
                }
                else
                {
                    ++lir_count_;
                }
                if (was_resident)
                    queue_.erase(entry.queue_it);
                hir_stack_.erase(entry.hir_it);
                stack_.splice(stack_.begin(), stack_, entry.stack_it);
                entry.lir = true;
            }

            void Evict(const KeyT& key)
            {
                auto hit = entries_.find(key);
                auto& entry = hit->second;
                queue_.erase(entry.queue_it);
                entry.page.reset();
                if (!entry.in_stack)
                    entries_.erase(hit);
            }

            void DropFromStack(const KeyT& key)
            {
                auto hit = entries_.find(key);
                auto& entry = hit->second;
                stack_.erase(entry.stack_it);
                hir_stack_.erase(entry.hir_it);
                entry.in_stack = false;
                if (!entry.page)
                    entries_.erase(hit);
            }

            void PruneStack()
            {
                while (!stack_.empty() && !entries_.at(stack_.back()).lir)
                    DropFromStack(stack_.back());
                while (stack_.size() > capacity_ && stack_.size() - capacity_ > capacity_)
                    DropFromStack(hir_stack_.back());
            }

            std::size_t capacity_;
            std::size_t lir_capacity_;
            std::size_t lir_count_ = 0;
            KeyList stack_;
            KeyList queue_;
            KeyList hir_stack_;
            std::unordered_map<KeyT, Entry> entries_;
    };
}
