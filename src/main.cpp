#include <iostream>
#include <exception>
#include <string>

#include "config.hpp"
#include "experiment.hpp"
#include "ideal.hpp"


int main(int argc, char* argv[])
{
    if (argc != 2 && argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <config> [--compare]\n";
        return 1;
    }

    try
    {
        const bool compare = argc == 3;
        if (compare && std::string(argv[2]) != "--compare")
        {
            std::cerr << "ERROR: unknown option: " << argv[2] << '\n';
            return 1;
        }

        const caches::Config config = caches::ReadConfig(argv[1]);
        if (compare)
        {
            caches::ExperimentInput input = caches::ReadExperimentInput(std::cin);
            const std::size_t hierarchy_hits = caches::CountHierarchyHits(config, &input);
            const std::size_t ideal_hits = caches::CountIdealHits(&input);

            std::cout << "hierarchy_hits: " << hierarchy_hits << '\n';
            std::cout << "ideal_hits: " << ideal_hits << '\n';
        }
        else
        {
            const std::size_t hits = caches::RunExperiment(config);
            std::cout << hits << '\n';
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    
    
    return 0;
}

