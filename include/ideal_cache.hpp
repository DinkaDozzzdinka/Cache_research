#pragma once

#include "cache_types.hpp"

#include <cstddef>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>


namespace caches {

    template <typename T, typename KeyT = Key>
    class IdealCache {
    private:
        struct Entry {
            T value;
            std::size_t next_request_pos;
        };

        using PageIndex = std::unordered_map<KeyT, Entry>;
        PageIndex pages_;

        std::size_t capacity_;
        std::size_t current_request_position = 0;

        std::vector<KeyT> requests_;
        std::vector<std::size_t> next_request_positions_;

        // define each key future position
        void build_new_request_positions() {
            const std::size_t no_next_request = requests_.size();
            next_request_positions_.assign(requests_.size(), no_next_request);       

            std::unordered_map<KeyT, std::size_t> nearest_position;

            for (std::size_t i = requests_.size(); i > 0;) {
                --i;

                const KeyT& key = requests_[i];
                auto found = nearest_position.find(key);

                if (found != nearest_position.end()) {
                    next_request_positions_[i] = found->second;
                }

                nearest_position[key] = i;
            }
        }

        // precondition: size ! = 0
        typename PageIndex::iterator find_page_to_evict() {
            auto page_to_evict = pages_.begin();

            for (auto current_page = pages_.begin(); current_page != pages_.end(); ++current_page) {
                if (current_page->second.next_request_pos > page_to_evict->second.next_request_pos) {
                    page_to_evict = current_page;
                }
            }

            return page_to_evict;
        }

    public:
        IdealCache(std::size_t capacity, const std::vector<KeyT>& requests)
            : capacity_(capacity),
            requests_(requests) 
            {
                build_new_request_positions();
            }

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
            if (current_request_position >= requests_.size()) {
                throw std::out_of_range("IdealCache: request sequence is completed");
            }

            if (!(requests_[current_request_position] == key)) {
                throw std::invalid_argument("IdealCache: request does not match the prepared sequence");
            }

            const std::size_t next_position = next_request_positions_[current_request_position];
            auto found = pages_.find(key);

            // hit
            if (found != pages_.end()) {
                const Entry& cached_entry = found->second;
                T page = cached_entry.value;

                found->second.next_request_pos = next_position;
                ++current_request_position;

                return {std::move(page), true};
            }

            // miss
            T page = load_page(key);

            if (capacity_ != 0) {
                if (pages_.size() == capacity_) {
                    auto page_to_evict = find_page_to_evict();
                    if (page_to_evict == pages_.end()) {
                        throw std::logic_error("IdealCache: no page to evict when cache is full");
                    }
                    
                    KeyT evicted_key = page_to_evict->first;

                    pages_.emplace(key, Entry{page, next_position});
                    pages_.erase(evicted_key);
                } else {
                    pages_.emplace(key, Entry{page, next_position});
                }
            }

            ++current_request_position;
            return {std::move(page), false};
        }
    };

} 