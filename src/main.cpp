#include <iostream>
#include <exception>

#include "config.hpp"
#include "experiment.hpp"


int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "ERROR: invalid number of argument in CL\n";
        return 1;
    }
    
    try
    {
        const caches:: Config config = caches:: ReadConfig(argv[1]);
        size_t hits = caches:: RunExperiment (config, std:: cin);
        std:: cout << hits << '\n';
        
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    
    
    return 0;
}

