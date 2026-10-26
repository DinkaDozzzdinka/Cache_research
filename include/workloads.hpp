#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace caches
{
    inline void CheckKeyRange(int first, std::size_t count)
    {
        if (first < 0 || count > static_cast<std::size_t>(std::numeric_limits<int>::max() - first) + 1)
            throw std::invalid_argument("Workload keys do not fit in int");
    }

    inline std::vector<int> GenerateSequential(std::size_t count, int first = 0)
    {
        CheckKeyRange(first, count);
        std::vector<int> requests;
        requests.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
            requests.push_back(first + static_cast<int>(index));
        return requests;
    }

    inline std::vector<int> GenerateCycle(std::size_t count, int working_set, int first = 0)
    {
        if (working_set <= 0)
            throw std::invalid_argument("Working set must be positive");
        CheckKeyRange(first, static_cast<std::size_t>(working_set));
        std::vector<int> requests;
        requests.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
            requests.push_back(first + static_cast<int>(index % static_cast<std::size_t>(working_set)));
        return requests;
    }

    inline std::vector<int> GenerateUniform(std::size_t count, int working_set, std::uint32_t seed)
    {
        if (working_set <= 0)
            throw std::invalid_argument("Working set must be positive");
        std::mt19937 engine(seed);
        std::uniform_int_distribution<int> distribution(0, working_set - 1);
        std::vector<int> requests;
        requests.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
            requests.push_back(distribution(engine));
        return requests;
    }

    inline std::vector<int> GeneratePhaseChange(std::size_t count, int working_set)
    {
        if (working_set <= 0 || working_set > std::numeric_limits<int>::max() / 2)
            throw std::invalid_argument("Invalid phase working set");
        std::vector<int> requests;
        requests.reserve(count);
        const auto middle = count / 2;
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto position = index < middle ? index : index - middle;
            const int key = static_cast<int>(position % static_cast<std::size_t>(working_set));
            requests.push_back(index < middle ? key : working_set + key);
        }
        return requests;
    }

    inline std::vector<int> GenerateHotScan(std::size_t count, int hot_size,
                                           std::size_t hot_requests, std::size_t scan_size)
    {
        if (hot_size <= 0 || hot_requests == 0 || scan_size == 0)
            throw std::invalid_argument("Hot set and both block sizes must be positive");
        CheckKeyRange(hot_size, count);
        std::vector<int> requests;
        requests.reserve(count);
        int next_scan_key = hot_size;
        for (;;)
        {
            for (std::size_t index = 0; index < hot_requests && requests.size() < count; ++index)
                requests.push_back(static_cast<int>(index % static_cast<std::size_t>(hot_size)));
            for (std::size_t index = 0; index < scan_size && requests.size() < count; ++index)
            {
                requests.push_back(next_scan_key);
                if (next_scan_key != std::numeric_limits<int>::max())
                    ++next_scan_key;
            }
            if (requests.size() == count)
                return requests;
        }
    }
}
