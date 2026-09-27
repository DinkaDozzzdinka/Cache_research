#pragma once

#include "cache_level.hpp"
#include "cache_types.hpp"
#include "config.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>


namespace caches 
{
    template <typename T>
    class CacheHierarchy {
    private:
        std::vector<CacheLevel<T>> levels_;

        template <typename Loader>
        T fetch(
            std::size_t level,
            const Key& key,
            Loader& source,
            bool& system_hit
        ) 
        {
            if (level == levels_.size()) {
                return source(key);             // slow_get_page
            }

            auto load_from_next = [&](const Key& requested_key) {
                return fetch(
                    level + 1,
                    requested_key,
                    source,
                    system_hit
                );
            };

            // NOTE: we can use std::visit later instead 
            auto result = access_level(
                levels_[level_number],
                key,
                load_from_next
            );

            if (result.hit) {
                system_hit = true;
            }

            return std::move(result.value);
        }

    public:
        CacheHierarchy(
            const Config& config,
            const std::vector<std::size_t>& capacities
        ) 
        {
            if (config.policies.empty()) {
                throw std::invalid_argument("No cache levels");
            }

            if (config.policies.size() != capacities.size()) {
                throw std::invalid_argument(
                    "Policy count and capacity count differ"
                );
            }

            levels_.reserve(config.policies.size());

            for (std::size_t i = 0; i < config.policies.size(); ++i) {
                levels_.push_back(
                    make_level<T>(
                        config.policies[i],
                        capacities[i]
                    )
                );
            }
        }

        template <typename Loader>
        LookupResult<T> lookup_update(
            const Key& key,
            Loader&& source
        ) {
            bool system_hit = false;

            T page = fetch(0, key, source, system_hit);

            return {std::move(page), system_hit};
        }

        std::size_t level_count() const noexcept {
            return levels_.size();
        }
    };
} 