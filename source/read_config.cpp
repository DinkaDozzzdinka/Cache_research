#include <charconv>
#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "config.hpp"

constexpr int MAX_CACHE_LEVELS = 64;

namespace caches
{
    Config ReadConfig(const std::string& filepath)
    {
        std::ifstream file(filepath);

        if (!file.is_open())
            throw std::runtime_error("Cannot open config file: " + filepath);

        std::string level_token;
        if (!(file >> level_token))
            throw std::runtime_error("Config is empty or cannot be read");
        
        std::size_t level_count = 0;

        const char* level_token_begin = level_token.data();
        const char* level_token_end = level_token_begin + level_token.size();
        const auto result = std::from_chars(level_token_begin, level_token_end, level_count);

        if (result.ec == std::errc::result_out_of_range)
            throw std::runtime_error("Number of cache levels is too large");

        if (result.ec != std::errc{} || result.ptr != level_token_end)
            throw std::runtime_error("Invalid number of cache levels: " + level_token);

        if (level_count == 0)
            throw std::runtime_error("Number of cache levels must be greater than 0");

        if (level_count > MAX_CACHE_LEVELS)
            throw std::runtime_error("Number of cache levels exceeds the limit of " + std::to_string(MAX_CACHE_LEVELS));
    
        Config config;
        config.policies.resize(level_count);

        for (std::size_t i = 0; i < level_count; ++i)
        {
            std::string cache_name;

            if (!(file >> cache_name))
                throw std::runtime_error("Missing cache policy for level "+ std::to_string(i + 1));

            config.policies[i] = ParsePolicy(cache_name);
        }

        std::string extra_token;

        if (file >> extra_token)
            throw std::runtime_error("Unexpected data after cache policies: " + extra_token);

        if (file.bad())
            throw std::runtime_error("Error while reading config file: " + filepath);
        
        return config;
    }
}