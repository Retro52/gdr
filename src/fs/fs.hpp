#pragma once

#include <types.hpp>
#include <bytes.hpp>

#include <fs/path.hpp>

#include <fstream>
#include <vector>

namespace fs
{
    inline bytes read_file(const fs::path& path)
    {
        std::ifstream file(path.c_str(), std::ios::binary | std::ios::ate);

        const auto size = file.tellg();
        bytes data(size);
        file.seekg(0, std::ios::beg);
        file.read(data.get<char>(), size);

        return data;
    }
}
