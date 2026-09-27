#pragma once

#include <unordered_map>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

// 3 LRU ARC LFU
// policies[0] - L1
// policies[1] - L2
// policies[2] - L3


namespace caches 
{
    enum class Policy 
    {
        LRU,
        ARC,
        TWO_Q,
        LFU,
        LIRS
    };

    inline Policy ParsePolicy (const std::string& cache_name) 
    {
        static const std:: unordered_map <std::string, Policy> policy_map = 
        {
            {"LRU",  Policy::LRU},
            {"ARC",  Policy::ARC},
            {"2Q",   Policy::TWO_Q},
            {"LFU",  Policy::LFU},
            {"LIRS", Policy::LIRS}
        };

        auto cache = policy_map.find (cache_name);
        if (cache == policy_map.end())
            throw std::invalid_argument("Unknown cache policy: " + cache_name);
        return cache->second;
    }

    struct Config 
    {
        std::vector<Policy> policies;
    };
    
    Config ReadConfig (const std::string& filepath);
} 

