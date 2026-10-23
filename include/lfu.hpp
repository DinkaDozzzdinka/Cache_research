#pragma once

#include <optional>
#include <cstddef>
#include <map>
#include <list>
#include <unordered_map>

namespace  caches
{
    template <typename T, typename KeyT = int>
    class LFUCache
    {
        public:

        explicit LFUCache (std:: size_t capacity): capacity_(capacity) {}

        LFUCache(const LFUCache&) = delete;
        LFUCache& operator=(const LFUCache&) = delete;

        template <typename H, typename G>
        void insert (KeyT key, T page, H CallBack, G TransferContext)
        {
            if (capacity_ == 0)
            {
                CallBack(key, page, TransferContext);
                return;
            }

            if (!full())
            {
                try
                {
                    frequency_groups_[1].emplace_front(key);
                }
                catch (...)
                {
                    auto group = frequency_groups_.find(1);
                    if (group != frequency_groups_.end() && (group -> second).empty())
                        frequency_groups_.erase(1);
                    throw;
                }
                try
                {
                    Entry entry = {page, 1, frequency_groups_[1].begin()};
                    hash_table_.emplace(key, entry);
                } 
                catch (...)
                {
                    frequency_groups_[1].pop_front();
                    if (frequency_groups_[1].empty())
                        frequency_groups_.erase(1);
                    throw;
                }
                return;                
            }

            auto min_group = frequency_groups_.begin();
            auto long_used_key = min_group -> second.back();
            T long_used_page = hash_table_.find(long_used_key) -> second.page;

            try
            {
                frequency_groups_[1].emplace_front(key);
            }
            catch (...)
            {
                auto group = frequency_groups_.find(1);
                if (group != frequency_groups_.end() && (group -> second).empty())
                    frequency_groups_.erase(1);
                throw;
            }

            try
            {
                Entry entry = {page, 1, frequency_groups_[1].begin()};
                hash_table_.emplace(key, entry);
            }
            catch (...)
            {
                frequency_groups_[1].pop_front();
                if (frequency_groups_[1].empty())
                    frequency_groups_.erase(1);
                throw;
            }

            min_group -> second.pop_back();
            if (min_group -> second.empty())
                frequency_groups_.erase(min_group);
            hash_table_.erase(long_used_key);
            CallBack(long_used_key, long_used_page, TransferContext);
            return;
        }

        template <typename F, typename H, typename G>
        bool lookup_update (KeyT key, F SlowGetPage, H CallBack, G TransferContext)
        {
            auto hit = hash_table_.find(key);
            if (hit == hash_table_.end())
            {
                T page = SlowGetPage(key);
                insert (key, page, CallBack, TransferContext);
                return false;
            }

            auto& entry = hit -> second;
            auto old_group = frequency_groups_.find(entry.frequency);
            auto& new_list = frequency_groups_[entry.frequency + 1];
            new_list.splice(new_list.begin(), old_group -> second, entry.keyIt);
            entry.frequency ++;
            if (old_group -> second.empty())
                frequency_groups_.erase(old_group);
            return true;
        }

        
        bool full() const noexcept {
            return hash_table_.size() >= capacity_;
        }
        
        bool empty() const noexcept {
            return hash_table_.empty();
        }
        
        std:: size_t size() const noexcept {
            return hash_table_.size();
        }
        
        std:: size_t capacity() const noexcept {
            return capacity_;
        }
        
        std:: optional<T> extract (KeyT key)
        {
            auto hash_key = hash_table_.find(key); 
            if (hash_key == hash_table_.end())
                return std:: nullopt;
            auto entry = hash_key -> second;
            auto group_certain_frequency = frequency_groups_.find(entry.frequency);
            
            group_certain_frequency -> second.erase(entry.keyIt);
            if (group_certain_frequency -> second.empty())
                frequency_groups_.erase(group_certain_frequency);
            hash_table_.erase(hash_key);
            return entry.page;
        }
        private:
        
            using ListIt = typename std:: list <KeyT>:: iterator;

            struct Entry
            {
                T page;
                std:: size_t frequency;
                ListIt keyIt;
            };

            std:: size_t capacity_;
            std:: unordered_map <KeyT, Entry> hash_table_;
            std:: map <std:: size_t, std:: list<KeyT>> frequency_groups_;        
    };

}
