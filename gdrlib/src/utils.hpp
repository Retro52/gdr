#pragma once

#include <cpp/containers/stack_string.hpp>

namespace utils
{
    cpp::stack_string format_bytes_number(u64 number);

    cpp::stack_string format_counter_number(u64 number);
}
