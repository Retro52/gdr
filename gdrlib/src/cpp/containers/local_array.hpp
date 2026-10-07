#pragma once

#include <pod_types.hpp>
#include <small_vector.hpp>

namespace cpp
{
    template<typename T, u64 kLocalCapacity = 64>
    using local_array = gch::small_vector<T, kLocalCapacity>;
}
