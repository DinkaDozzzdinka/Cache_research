#pragma once

#include <iosfwd>
#include "config.hpp"

namespace caches
{
    // Read the input, run the hierarchy and IdealCache, then write statistics.
    // Exceptions are handled by the caller (main)
    void RunExperiment(const Config& config, std::istream& input, std::ostream& output);
} 