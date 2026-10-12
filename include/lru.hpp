#pragma once


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

            template <typename F>
            bool lookup_update (KeyT key, F slow_get_page)
            {
                if (capacity_ == 0)
                    return false;

                auto hit = hash_table_.find(key);
                if (hit != hash_table_.end())
                {
                    auto node_pointer = hit->second;
                    items_list_.splice(items_list_.begin(), items_list_, node_pointer);
                    return true;
                }

                if (full())
                {
                    hash_table_.erase(items_list_.back().first);
                    items_list_.pop_back();
                }
                
                T page = slow_get_page(key);
                items_list_.emplace_front(key, page);
                hash_table_.emplace(key, items_list_.begin());
                return false;
                
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

        private:
            size_t capacity_;
            std:: unordered_map <KeyT, ListIt> hash_table_;
            std:: list <std:: pair <KeyT, T>> items_list_;
    };
}