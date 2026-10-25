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
    class ARCCache
    {
        public:
            explicit ARCCache(std::size_t capacity) : capacity_(capacity) {}

            ARCCache(const ARCCache&) = delete;
            ARCCache& operator=(const ARCCache&) = delete;

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
                t2_.splice(t2_.begin(), Keys(entry.queue), entry.key_it);
                entry.queue = Queue::T2;
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
                    throw std::invalid_argument("ARC insert requires an absent page");

                const bool in_b1 = known != entries_.end() && known->second.queue == Queue::B1;
                const bool in_b2 = known != entries_.end() && known->second.queue == Queue::B2;
                std::size_t next_target = target_;
                std::optional<KeyT> forgotten;
                std::optional<std::pair<KeyT, T>> evicted;
                bool keep_victim_history = true;

                if (in_b1)
                {
                    const auto delta = std::max(std::size_t{1}, b2_.size() / b1_.size());
                    next_target += std::min(delta, capacity_ - next_target);
                }
                else if (in_b2)
                {
                    const auto delta = std::max(std::size_t{1}, b1_.size() / b2_.size());
                    next_target -= std::min(delta, next_target);
                }
                else if (t1_.size() + b1_.size() == capacity_)
                {
                    if (t1_.size() < capacity_)
                        forgotten = b1_.back();
                    else
                    {
                        const auto& victim_key = t1_.back();
                        evicted.emplace(victim_key, *entries_.at(victim_key).page);
                        keep_victim_history = false;
                    }
                }
                else if (entries_.size() >= capacity_ && entries_.size() - capacity_ == capacity_)
                {
                    forgotten = b2_.back();
                }

                if (full() && !evicted)
                {
                    const bool from_t1 = !t1_.empty() && (t2_.empty() ||
                        t1_.size() > next_target || (in_b2 && t1_.size() == next_target));
                    const auto& victim_key = from_t1 ? t1_.back() : t2_.back();
                    evicted.emplace(victim_key, *entries_.at(victim_key).page);
                }

                if (known != entries_.end())
                {
                    auto& entry = known->second;
                    entry.page = std::make_unique<T>(page);
                    t2_.splice(t2_.begin(), Keys(entry.queue), entry.key_it);
                    entry.queue = Queue::T2;
                }
                else
                {
                    t1_.push_front(key);
                    try
                    {
                        entries_.emplace(key, Entry{std::make_unique<T>(page), Queue::T1, t1_.begin()});
                    }
                    catch (...)
                    {
                        t1_.pop_front();
                        throw;
                    }
                }

                target_ = next_target;
                if (forgotten)
                    Erase(entries_.find(*forgotten));
                if (evicted)
                {
                    auto victim = entries_.find(evicted->first);
                    if (keep_victim_history)
                    {
                        auto& entry = victim->second;
                        const auto destination = entry.queue == Queue::T1 ? Queue::B1 : Queue::B2;
                        auto& history = Keys(destination);
                        history.splice(history.begin(), Keys(entry.queue), entry.key_it);
                        entry.page.reset();
                        entry.queue = destination;
                    }
                    else
                    {
                        Erase(victim);
                    }
                    CallBack(evicted->first, evicted->second, TransferContext);
                }
            }

            std::optional<T> extract(KeyT key)
            {
                auto hit = entries_.find(key);
                if (hit == entries_.end() || !hit->second.page)
                    return std::nullopt;
                T page = *hit->second.page;
                Erase(hit);
                return page;
            }

            std::size_t size() const noexcept { return t1_.size() + t2_.size(); }
            std::size_t capacity() const noexcept { return capacity_; }
            bool empty() const noexcept { return size() == 0; }
            bool full() const noexcept { return size() >= capacity_; }

        private:
            enum class Queue { T1, T2, B1, B2 };
            using KeyList = std::list<KeyT>;
            using KeyIt = typename KeyList::iterator;

            struct Entry
            {
                std::unique_ptr<T> page;
                Queue queue;
                KeyIt key_it;
            };

            using Table = std::unordered_map<KeyT, Entry>;

            KeyList& Keys(Queue queue)
            {
                switch (queue)
                {
                    case Queue::T1: return t1_;
                    case Queue::T2: return t2_;
                    case Queue::B1: return b1_;
                    case Queue::B2: return b2_;
                }
                throw std::logic_error("Invalid ARC queue");
            }

            void Erase(typename Table::iterator entry)
            {
                Keys(entry->second.queue).erase(entry->second.key_it);
                entries_.erase(entry);
            }

            std::size_t capacity_;
            std::size_t target_ = 0;
            KeyList t1_;
            KeyList t2_;
            KeyList b1_;
            KeyList b2_;
            Table entries_;
    };
}
