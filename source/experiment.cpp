#include "experiment.hpp"

#include <charconv>
#include <cstddef>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "cache_hierarchy.hpp"
#include "cache_types.hpp"
#include "ideal_cache.hpp"


namespace
{
    struct ExperimentPage
    {
        caches::Key id;
    };

    struct PageLoader
    {
        std::size_t load_count = 0;

        ExperimentPage operator()(const caches::Key& key)
        {
            ++load_count;
            return ExperimentPage{key};
        }
    };

    struct RunStats
    {
        std::size_t hits = 0;
        std::size_t source_loads = 0;
    };


    template <typename Number>
    Number ReadInteger(std::istream& input, const std::string& err_msg)
    {
        std::string token;

        if (!(input >> token))
        {
            if (input.bad())
                throw std::runtime_error("Input read error");

            throw std::invalid_argument("Missing " + err_msg);
        }

        Number value{};
        const char* begin = token.data();
        const char* end = begin + token.size();
        const auto result = std::from_chars(begin, end, value);

        if (result.ec == std::errc::result_out_of_range)
        {
            throw std::out_of_range(err_msg + " is out of range: " + token);
        }

        if (result.ec != std::errc{} || result.ptr != end)
        {
            throw std::invalid_argument("Invalid " + err_msg + ": " + token);
        }

        return value;
    }


    std::vector<caches::Key> ReadRequests(std::istream& input, std::size_t request_count)
    {
        std::vector<caches::Key> requests;

        for (std::size_t i = 0; i < request_count; ++i) {
            requests.push_back(ReadInteger<caches::Key>(input, "request at position " + std::to_string(i + 1)));
        }

        std::string extra_token;

        if (input >> extra_token)
            throw std::invalid_argument("Unexpected data after requests: " + extra_token);

        if (input.bad())
            throw std::runtime_error("Input read error");

        return requests;
    }


    std::vector<std::size_t> SplitCapacity(std::size_t total_capacity, std::size_t level_count)
    {
        if (level_count == 0)
            throw std::invalid_argument("No cache levels");

        if (total_capacity < level_count)
            throw std::invalid_argument("Total capacity must give every level at least one page");
    
        std::vector<std::size_t> capacities(level_count, total_capacity / level_count);

        const std::size_t remainder = total_capacity % level_count;

        // Assign the remaining pages to the deepest levels.
        for (std::size_t i = level_count - remainder; i < level_count; ++i) {
            ++capacities[i];
        }

        return capacities;
    }


    const char* GetPolicyName(caches::Policy policy)
    {
        switch (policy)
        {
            case caches::Policy::LRU:       return "LRU";
            case caches::Policy::ARC:       return "ARC";
            case caches::Policy::TWO_Q:     return "2Q";
            case caches::Policy::LFU:       return "LFU";
            case caches::Policy::LIRS:      return "LIRS";
            default:
                throw std::invalid_argument("Unknown cache policy value");
        }
    }


    // one run of the sequence through the cache
    template <typename Cache>
    RunStats RunRequests(Cache& cache, const std::vector<caches::Key>& requests)
    {
        PageLoader source;
        RunStats stats;

        for (const auto& key : requests)
        {
            const auto result = cache.lookup_update(key, source);

            if (result.hit)
                ++stats.hits;
        }

        stats.source_loads = source.load_count;

        if (stats.source_loads != requests.size() - stats.hits)
            throw std::logic_error("Cache hits and source loads do not match the request count");

        return stats;
    }
} 



namespace caches
{
    void RunExperiment(const Config& config, std::istream& input, std::ostream& output)
    {
        const std::size_t total_capacity = ReadInteger<std::size_t>(input, "total capacity");
        const std::size_t request_count  = ReadInteger<std::size_t>(input, "request count");
        const auto level_count = config.policies.size();

        if (request_count == 0)
            throw std::invalid_argument("Experiment must contain at least one request");


        const auto capacities = SplitCapacity(total_capacity, level_count);
        const auto requests   = ReadRequests(input, request_count);


        caches::CacheHierarchy<ExperimentPage> hierarchy(config, capacities);
        const RunStats actual_stats = RunRequests(hierarchy, requests);

        caches::IdealCache<ExperimentPage> ideal(total_capacity, requests);
        const RunStats reference_stats = RunRequests(ideal, requests);

        
        output << "policies=";
        for (std::size_t i = 0; i < config.policies.size(); ++i)
        {
            if (i != 0)
                output << ' ';
            output << GetPolicyName(config.policies[i]);
        }

        output << "\ncapacities=";
        for (std::size_t i = 0; i < capacities.size(); ++i)
        {
            if (i != 0)
                output << ' ';
            output << capacities[i];
        }

        output  << "\nlevels=" << hierarchy.level_count()
                << "\ntotal_capacity=" << total_capacity
                << "\nrequests=" << request_count
                << "\nsystem_hits=" << actual_stats.hits
                << "\nsource_loads=" << actual_stats.source_loads
                << "\nideal_hits=" << reference_stats.hits
                << "\nideal_source_loads=" << reference_stats.source_loads
                << std::fixed << std::setprecision(6)
                << "\nhit_rate="
                << static_cast<double>(actual_stats.hits) / static_cast<double>(request_count)
                << "\nideal_hit_rate="
                << static_cast<double>(reference_stats.hits) / static_cast<double>(request_count)
                << '\n';
    }
} 
