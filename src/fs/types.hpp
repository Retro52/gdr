#pragma once

#include <cpp/string/stack_string.hpp>

#include <filesystem>

namespace fs
{
    constexpr u64 kMaxPathLength = 260;
    using path_string            = cpp::stack_string_base<kMaxPathLength>;

    constexpr char path_separator   = '/';
    constexpr char path_terminator  = '\0';
    constexpr char native_separator = std::filesystem::path::preferred_separator;
}
