#include <exception>
#include <iostream>

#include "config.hpp"
#include "experiment.hpp"


int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0]  << " <config_file> < requests.in\n";
        return 1;
    }

    try
    {
        const caches::Config config = caches::ReadConfig(argv[1]);
        caches::RunExperiment(config, std::cin, std::cout);
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}

