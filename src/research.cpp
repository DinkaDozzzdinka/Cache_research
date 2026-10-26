#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "experiment.hpp"
#include "research.hpp"
#include "workloads.hpp"

namespace
{
    struct Options
    {
        std::filesystem::path output = "results/full";
        std::size_t requests = 10000;
        std::size_t max_levels = 3;
        std::vector<std::size_t> capacities{8, 16, 32};
        std::vector<std::uint32_t> train_seeds{42, 43, 44};
        std::vector<std::uint32_t> validation_seeds{101, 102};
    };

    std::size_t ParseNumber(const std::string& text)
    {
        std::size_t value = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
            throw std::invalid_argument("Invalid nonnegative integer: " + text);
        return value;
    }

    std::vector<std::size_t> ParseList(const std::string& text)
    {
        std::vector<std::size_t> values;
        std::size_t start = 0;
        for (;;)
        {
            const auto end = text.find(',', start);
            const auto value = ParseNumber(text.substr(start, end == std::string::npos ? end : end - start));
            if (std::find(values.begin(), values.end(), value) != values.end())
                throw std::invalid_argument("Repeated value in list");
            values.push_back(value);
            if (end == std::string::npos)
                return values;
            start = end + 1;
        }
    }

    std::vector<std::uint32_t> ParseSeeds(const std::string& text)
    {
        std::vector<std::uint32_t> seeds;
        for (auto value : ParseList(text))
        {
            if (value > std::numeric_limits<std::uint32_t>::max())
                throw std::invalid_argument("Seed exceeds uint32_t");
            seeds.push_back(static_cast<std::uint32_t>(value));
        }
        return seeds;
    }

    struct Dataset
    {
        std::string id;
        std::string workload;
        std::string split;
        std::string seed;
        int working_set;
        std::size_t hot_requests;
        std::size_t scan_size;
        std::vector<int> requests;
    };

    std::vector<Dataset> MakeDatasets(const Options& options)
    {
        std::vector<Dataset> datasets;
        for (const std::string split : {"train", "validation"})
        {
            const bool train = split == "train";
            const int hot_size = train ? 4 : 6;
            const int cycle_size = train ? 33 : 35;
            const int phase_size = train ? 8 : 10;
            const std::size_t hot_requests = train ? 128 : 96;
            const std::size_t scan_size = train ? 64 : 96;
            datasets.push_back({split + "_sequential_unique", "sequential_unique", split, "", 0, 0, 0,
                                caches::GenerateSequential(options.requests)});
            datasets.push_back({split + "_hot_set", "hot_set", split, "", hot_size, 0, 0,
                                caches::GenerateCycle(options.requests, hot_size)});
            datasets.push_back({split + "_cycle", "cycle", split, "", cycle_size, 0, 0,
                                caches::GenerateCycle(options.requests, cycle_size)});
            datasets.push_back({split + "_hot_scan", "hot_scan", split, "", hot_size, hot_requests, scan_size,
                                caches::GenerateHotScan(options.requests, hot_size, hot_requests, scan_size)});
            datasets.push_back({split + "_phase_change", "phase_change", split, "", phase_size, 0, 0,
                                caches::GeneratePhaseChange(options.requests, phase_size)});
            const auto& seeds = train ? options.train_seeds : options.validation_seeds;
            for (auto seed : seeds)
                datasets.push_back({split + "_uniform_" + std::to_string(seed), "uniform", split,
                                    std::to_string(seed), 64, 0, 0,
                                    caches::GenerateUniform(options.requests, 64, seed)});
        }
        return datasets;
    }

    std::ofstream OpenOutput(const std::filesystem::path& path)
    {
        std::ofstream output(path);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        return output;
    }

    void Run(const Options& options)
    {
        const auto configurations = caches::EnumerateConfigurations(options.max_levels);
        const auto datasets = MakeDatasets(options);
        std::filesystem::create_directories(options.output / "inputs");
        std::filesystem::create_directories(options.output / "configs");
        auto raw = OpenOutput(options.output / "raw.csv");
        raw << "dataset,workload,split,seed,capacity,levels,policies,requests,hits,ideal_hits,"
               "hit_ratio,ideal_hit_ratio,hit_gap,gap_pp,level_capacities\n";
        raw << std::fixed << std::setprecision(10);
        auto parameters = OpenOutput(options.output / "datasets.csv");
        parameters << "dataset,workload,split,seed,requests,working_set,hot_requests,scan_size,input_file\n";
        auto configuration_file = OpenOutput(options.output / "configurations.csv");
        configuration_file << "policies,levels,capacity,level_capacities\n";

        for (const auto& config : configurations)
        {
            const auto name = caches::ConfigurationName(config);
            auto file = OpenOutput(options.output / "configs" / (name + ".cfg"));
            file << config.policies.size() << '\n';
            for (auto policy : config.policies)
                file << caches::PolicyName(policy) << ' ';
            file << '\n';
            for (auto capacity : options.capacities)
                configuration_file << name << ',' << config.policies.size() << ',' << capacity << ','
                                   << caches::LevelCapacities(capacity, config.policies.size()) << '\n';
        }

        for (const auto& dataset : datasets)
        {
            const std::string input_file = "inputs/" + dataset.id + ".in";
            auto input = OpenOutput(options.output / input_file);
            input << options.capacities.front() << ' ' << dataset.requests.size() << '\n';
            for (int key : dataset.requests)
                input << key << ' ';
            input << '\n';
            input.close();
            parameters << dataset.id << ',' << dataset.workload << ',' << dataset.split << ',' << dataset.seed
                       << ',' << dataset.requests.size() << ',' << dataset.working_set << ',' << dataset.hot_requests
                       << ',' << dataset.scan_size << ',' << input_file << '\n';
            for (auto capacity : options.capacities)
            {
                caches::ExperimentInput experiment{capacity, dataset.requests};
                const auto ideal = caches::CountIdealHits(&experiment);
                for (const auto& config : configurations)
                {
                    const auto hits = caches::CountHierarchyHits(config, &experiment);
                    if (hits > ideal)
                        throw std::logic_error("Hierarchy exceeded ideal hit count");
                    const double count = static_cast<double>(experiment.requests.size());
                    raw << dataset.id << ',' << dataset.workload << ',' << dataset.split << ',' << dataset.seed
                        << ',' << capacity << ',' << config.policies.size() << ',' << caches::ConfigurationName(config)
                        << ',' << dataset.requests.size() << ',' << hits << ',' << ideal << ',' << hits / count
                        << ',' << ideal / count << ',' << ideal - hits << ',' << 100.0 * (ideal - hits) / count
                        << ',' << caches::LevelCapacities(capacity, config.policies.size()) << '\n';
                }
                std::cerr << dataset.id << ", capacity=" << capacity << ": "
                          << configurations.size() << " configurations\n";
            }
        }
        auto metadata = OpenOutput(options.output / "run_parameters.txt");
        metadata << "requests=" << options.requests << "\nmax_levels=" << options.max_levels
                 << "\nconfigurations=" << configurations.size() << "\ndatasets=" << datasets.size()
                 << "\ncompiler=" << __VERSION__ << "\ncplusplus=" << __cplusplus
                 << "\ninitial_state=empty\nmetric=hit_ratio\n";
        raw.close();
        parameters.close();
        configuration_file.close();
        metadata.close();
    }
}

int main(int argc, char* argv[])
{
    try
    {
        Options options;
        for (int index = 1; index < argc; ++index)
        {
            const std::string name = argv[index];
            if (name == "--help")
            {
                std::cout << "cache_research [--output DIR] [--requests N] [--max-levels 1..3] "
                             "[--capacities 8,16,32] [--train-seeds 42,43,44] [--validation-seeds 101,102]\n";
                return 0;
            }
            if (index + 1 >= argc)
                throw std::invalid_argument("Missing option value: " + name);
            const std::string value = argv[++index];
            if (name == "--output") options.output = value;
            else if (name == "--requests") options.requests = ParseNumber(value);
            else if (name == "--max-levels") options.max_levels = ParseNumber(value);
            else if (name == "--capacities") options.capacities = ParseList(value);
            else if (name == "--train-seeds") options.train_seeds = ParseSeeds(value);
            else if (name == "--validation-seeds") options.validation_seeds = ParseSeeds(value);
            else throw std::invalid_argument("Unknown research option: " + name);
        }
        if (options.requests == 0)
            throw std::invalid_argument("Research needs at least one request");
        for (auto seed : options.train_seeds)
            if (std::find(options.validation_seeds.begin(), options.validation_seeds.end(), seed) != options.validation_seeds.end())
                throw std::invalid_argument("Training and validation seeds must be disjoint");
        Run(options);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
