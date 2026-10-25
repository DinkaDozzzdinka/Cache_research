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
    class TwoQCache
    {
        public:
            explicit TwoQCache(std::size_t capacity): capacity_(capacity), in_target_(capacity == 0 ? 0 : std::max(std::size_t{1}, capacity / 4)), out_capacity_(capacity == 0 ? 0 : std::max(std::size_t{1}, capacity / 2)) {}

            TwoQCache(const TwoQCache&) = delete;
            TwoQCache& operator=(const TwoQCache&) = delete;

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
                if (hit == entries_.end() || hit->second.queue == Queue::Out)
                    return false;

                if (hit->second.queue == Queue::Main)
                    am_.splice(am_.begin(), am_, hit->second.key_it);
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
                if (known != entries_.end() && known->second.queue != Queue::Out)
                    throw std::invalid_argument("2Q insert requires an absent page");

                std::optional<std::pair<KeyT, T>> evicted;
                if (full())
                {
                    const bool evict_from_in = !a1in_.empty() && (a1in_.size() > in_target_ || am_.empty());
                    const KeyT& victim_key = evict_from_in ? a1in_.back() : am_.back();
                    evicted.emplace(victim_key, *entries_.at(victim_key).page);
                }

                if (known != entries_.end())
                {
                    auto& entry = known->second;
                    entry.page = std::make_unique<T>(page);
                    am_.splice(am_.begin(), a1out_, entry.key_it);
                    entry.queue = Queue::Main;
                }
                else
                {
                    a1in_.push_front(key);
                    try
                    {
                        entries_.emplace(key, Entry{std::make_unique<T>(page), Queue::In, a1in_.begin()});
                    }
                    catch (...)
                    {
                        a1in_.pop_front();
                        throw;
                    }
                }

                if (evicted)
                    Evict(evicted->first);
                TrimHistory();

                if (evicted)
                    CallBack(evicted->first, evicted->second, TransferContext);
            }

            std::optional<T> extract(KeyT key)
            {
                auto hit = entries_.find(key);
                if (hit == entries_.end() || hit->second.queue == Queue::Out)
                    return std::nullopt;

                T page = *hit->second.page;
                Erase(hit);
                return page;
            }

            std::size_t size() const noexcept
            {
                return a1in_.size() + am_.size();
            }

            std::size_t capacity() const noexcept
            {
                return capacity_;
            }

            bool empty() const noexcept
            {
                return size() == 0;
            }

            bool full() const noexcept
            {
                return size() >= capacity_;
            }

        private:
            enum class Queue { In, Main, Out };
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
                if (queue == Queue::In)
                    return a1in_;
                if (queue == Queue::Main)
                    return am_;
                return a1out_;
            }

            void Erase(typename Table::iterator entry)
            {
                Keys(entry->second.queue).erase(entry->second.key_it);
                entries_.erase(entry);
            }

            void Evict(const KeyT& key)
            {
                auto hit = entries_.find(key);
                auto& entry = hit->second;
                if (entry.queue == Queue::In)
                {
                    a1out_.splice(a1out_.begin(), a1in_, entry.key_it);
                    entry.page.reset();
                    entry.queue = Queue::Out;
                }
                else
                {
                    Erase(hit);
                }
            }

            void TrimHistory()
            {
                while (a1out_.size() > out_capacity_)
                    Erase(entries_.find(a1out_.back()));
            }

            std::size_t capacity_;
            std::size_t in_target_;
            std::size_t out_capacity_;
            KeyList a1in_;
            KeyList am_;
            KeyList a1out_;
            Table entries_;
    };
}
