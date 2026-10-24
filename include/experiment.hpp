#pragma once

#include <iostream>
#include <cstddef>
#include <deque>
#include <optional>
#include <stdexcept>

#include "cache_level.hpp"
#include "lru.hpp"
#include "config.hpp"

namespace caches
{
    struct page_t {
        int id;
    };
    
    using CacheLevels = std:: deque<CacheLevel<page_t, int>>;

    struct TransferContext
    {
        CacheLevels& levels;
        std:: size_t index_next_level;
    };

    inline page_t SlowGetPage(int id) {
        return page_t{id};
    }

    inline void CallBack(int key, page_t page, TransferContext *transfer_context)
    {
        if (transfer_context->index_next_level >= transfer_context->levels.size())
            return;
        auto& level = transfer_context->levels[transfer_context->index_next_level];
        TransferContext next_context{ transfer_context->levels, transfer_context->index_next_level + 1};
        level.insert(key, page, CallBack, &next_context);
    }

    inline size_t RunExperiment (const Config& config, std:: istream& in)
    {
        std::size_t capacity = 0;
        std::size_t counter_requests = 0;
        std::size_t hits = 0;

        if (!(in >> capacity >> counter_requests))
            throw std:: runtime_error ("Invalid input parameters\n");

        const std::size_t number_levels = config.policies.size();
        if (number_levels == 0)
            throw std:: invalid_argument("At least one cache level is required\n");

        CacheLevels levels;
        const std::size_t base_capacity = capacity / number_levels;
        const std::size_t extra_pages = capacity % number_levels;

        for (std::size_t level_index = 0; level_index < number_levels; ++level_index)
        {
            const std::size_t level_capacity = base_capacity + (level_index < extra_pages ? 1 : 0);
            levels.emplace_back(config.policies[level_index], level_capacity);
        }

        TransferContext transfer_context = {levels, 1};

        for (std::size_t request_index = 0; request_index < counter_requests; ++request_index)
        {
            int key = 0;
            if (!(in >> key))
            throw std:: runtime_error ("Invalid input argument\n");
            
            if (levels[0].touch(key))
            {
                hits++;
                continue;
            }
            std::optional<page_t> page;
            for (std:: size_t index_level = 1; index_level < levels.size(); index_level++)
            {
                page = levels[index_level].extract(key);
                if (page)
                {
                    hits++;
                    break;
                }
            }

            if (!page)
                page = SlowGetPage(key);
            levels[0].insert(key, *page, CallBack, &transfer_context);
        }

        return hits; 

    }
    
}
