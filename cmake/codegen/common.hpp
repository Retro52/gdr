#pragma once

#include <string_view>

namespace codegen
{
    // Type name traits
    template<typename T>
    constexpr std::string_view type_name = "Unknown";
}
