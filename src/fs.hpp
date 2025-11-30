#pragma once

#include <types.hpp>

#include <fstream>
#include <string_view>
#include <vector>

inline bytes read_file(std::string_view filename)
{
    std::ifstream file(filename.data(), std::ios::binary | std::ios::ate);

    const auto size = file.tellg();
    bytes data(size);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(data.data()), size);

    return data;
}
