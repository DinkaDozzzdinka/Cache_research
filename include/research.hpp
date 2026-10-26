#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "config.hpp"

namespace caches
{
    inline const char* PolicyName(Policy policy)
    {
        switch (policy)
        {
            case Policy::LRU: return "LRU";
            case Policy::LFU: return "LFU";
            case Policy::TWO_Q: return "2Q";
            case Policy::ARC: return "ARC";
            case Policy::LIRS: return "LIRS";
        }
        throw std::invalid_argument("Unknown cache policy");
    }

    inline std::string ConfigurationName(const Config& config)
    {
        std::string name;
        for (auto policy : config.policies)
        {
            if (!name.empty())
                name += '|';
            name += PolicyName(policy);
        }
        return name;
    }

    inline std::vector<Config> EnumerateConfigurations(std::size_t max_levels)
    {
        if (max_levels == 0 || max_levels > 3)
            throw std::invalid_argument("Research supports one to three levels");
        const std::vector<Policy> policies{Policy::LRU, Policy::LFU, Policy::TWO_Q, Policy::ARC, Policy::LIRS};
        std::vector<Config> result;
        std::vector<Config> current{Config{}};
        for (std::size_t depth = 0; depth < max_levels; ++depth)
        {
            std::vector<Config> next;
            for (const auto& prefix : current)
            {
                for (auto policy : policies)
                {
                    Config config = prefix;
                    config.policies.push_back(policy);
                    next.push_back(config);
                    result.push_back(config);
                }
            }
            current = next;
        }
        return result;
    }

    inline std::string LevelCapacities(std::size_t capacity, std::size_t levels)
    {
        if (levels == 0)
            throw std::invalid_argument("At least one level is required");
        std::string result;
        for (std::size_t index = 0; index < levels; ++index)
        {
            if (!result.empty())
                result += '|';
            result += std::to_string(capacity / levels + (index < capacity % levels ? 1 : 0));
        }
        return result;
    }
}
