#pragma once

#include <iostream>

#include "lru.hpp"
#include "config.hpp"

namespace caches
{
    struct page_t {
        int id;
    };

    inline page_t slow_get_page(int id) {
        return page_t{id};
    }

    inline void RunExperiment (const Config& config, std:: istream& in)
    {
        (void) config;
        size_t capacity = 0;
        size_t counter_requests = 0;
        int hits = 0;

        if (!(in >> capacity >> counter_requests))
            return;

        LRUCache <page_t, int> LRU(capacity);

        for (int i = 0; i < (int)counter_requests; i++)
        {
            page_t page;
            in >> page.id;
            if (LRU.lookup_update(page.id, slow_get_page))
            {
                hits += 1;
            }
        }

        std:: cout << hits << "\n"; 

    }
}