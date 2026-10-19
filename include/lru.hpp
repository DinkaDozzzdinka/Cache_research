#pragma once

#include <optional>
#include <list>
#include <unordered_map>

namespace caches
{
    template <typename T, typename KeyT = int>
    class LRUCache
    {
        public:
            using ListIt = typename std:: list <std:: pair <KeyT, T>>:: iterator;
            explicit LRUCache (size_t capacity): capacity_(capacity) {}

            LRUCache(const LRUCache&) = delete;
            LRUCache& operator=(const LRUCache&) = delete;

            template <typename F, typename H, typename G>
            bool lookup_update (KeyT key, F SlowGetPage, H CallBack, G TransferContext)
            {

                auto hit = hash_table_.find(key);
                if (hit != hash_table_.end())
                {
                    auto node_pointer = hit->second;
                    items_list_.splice(items_list_.begin(), items_list_, node_pointer);
                    return true;
                }
                
                T page = SlowGetPage(key);
                insert (key, page, CallBack, TransferContext);
                return false;
                
            }

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
                    items_list_.emplace_front(key, page);
                    try
                    {
                        hash_table_.emplace(key, items_list_.begin());
                    } 
                    catch (...)
                    {
                        items_list_.pop_front();
                        throw;
                    }
                    return;
                }

                auto old_page = items_list_.back();
                items_list_.emplace_front (key, page);
                try
                {
                    hash_table_.emplace(key, items_list_.begin());
                } 
                catch (...)
                {
                    items_list_.pop_front();
                    throw;
                }
                hash_table_.erase(old_page.first);
                items_list_.pop_back();
                
                CallBack(old_page.first, old_page.second, TransferContext);
                return;
            }

            bool full() const noexcept {
                return items_list_.size() >= capacity_;
            }

            bool empty() const noexcept {
                return items_list_.empty();
            }

            size_t size() const noexcept {
                return items_list_.size();
            }

            size_t capacity() const noexcept {
                return capacity_;
            }

            std:: optional<T> extract (KeyT key)
            {
                auto hash_key = hash_table_.find(key); 
                if (hash_key == hash_table_.end())
                    return std:: nullopt;
                auto list_it = hash_key -> second;
                T page = list_it -> second;
                items_list_.erase(list_it);
                hash_table_.erase(hash_key);
                return page;
            }

        private:
            size_t capacity_;
            std:: unordered_map <KeyT, ListIt> hash_table_;
            std:: list <std:: pair <KeyT, T>> items_list_;
    };
}