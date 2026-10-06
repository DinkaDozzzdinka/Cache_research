#include <iostream>


#include "config.hpp"


int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "ERROR: invalid number of argument in CL\n";
        return 1;
    }
    
    const caches::Config config = caches::ReadConfig(argv[1]);
    
    return 0;
}

