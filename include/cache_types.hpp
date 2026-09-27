#pragma once

namespace caches 
{
    using Key = int;
    struct Page {
        Key id;
    };

    template <typename T>
    struct LookupResult {
        T value;
        bool hit;
    };
} 