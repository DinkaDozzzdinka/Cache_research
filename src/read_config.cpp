#include "config.hpp"

namespace caches
{

    Config ReadConfig (const std::string& filepath)
    {
        std:: ifstream file(filepath);
        if (!file.is_open())
            throw std:: runtime_error("Invalid open config file\n");
        
        std:: string cache_level;
        if (!(file >> cache_level))
            throw std:: runtime_error("Empty config file\n");

        size_t number_cache_level = 0;
        auto [ptr, ec] = std:: from_chars(cache_level.data(), cache_level.data() + cache_level.size(), number_cache_level);

        if (ec != std:: errc{} || ptr != cache_level.data() + cache_level.size())
            throw std:: invalid_argument("Invalid recording cache_level\n");
        
        caches:: Config config;
        config.policies.resize(number_cache_level);
        std:: string cache_name;
        for (size_t element = 0; element < number_cache_level; element++)
        {
            if (!(file >> cache_name))
                throw std::runtime_error ("A few arguments in config file\n");
            caches:: Policy policy = caches:: ParsePolicy (cache_name);
            config.policies[element] = policy;
        }

        std:: string extra_policy;
        if (file >> extra_policy)
            throw std:: runtime_error("Unexpected extra policy in config file\n"); 

        return config;
    }

}