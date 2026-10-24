#pragma once

#include <cstddef>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace caches
{
    template <typename KeyT>
    std::size_t CountIdealHits(std::size_t capacity, const std::vector<KeyT>& requests)
    {
        if (capacity == 0)
            return 0;

        std::unordered_map<KeyT, std::deque<std::size_t>> future;
        std::unordered_set<KeyT> cached;

        for (std::size_t index = 0; index < requests.size(); ++index)
        {
            future[requests[index]].push_back(index);
        }

        std::size_t hits = 0;
        for (std::size_t index = 0; index < requests.size(); ++index)
        {
            const KeyT& key = requests[index];
            future.at(key).pop_front();

            if (cached.find(key) != cached.end())
            {
                ++hits;
                continue;
            }

            if (cached.size() == capacity)
            {
                auto victim = cached.begin();
                std::size_t farthest_position = 0;

                for (auto it = cached.begin(); it != cached.end(); ++it)
                {
                    const auto& positions = future.at(*it);
                    std::size_t next_position = positions.empty() ? requests.size() : positions.front();

                    if (next_position > farthest_position)
                    {
                        farthest_position = next_position;
                        victim = it;
                    }
                }

                cached.erase(victim);
            }

            cached.insert(key);
        }

        return hits;
    }
}
