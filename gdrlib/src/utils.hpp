#pragma once

#include <core/containers/stack_string.hpp>

namespace utils
{
    u64 align_divide(u64 number, u64 alignment);

    core::stack_string format_bytes_number(u64 number);

    core::stack_string format_counter_number(u64 number);
}
