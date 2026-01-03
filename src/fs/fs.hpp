#pragma once

#include <types.hpp>

#include <bytes.hpp>
#include <fs/path.hpp>
#include <result.hpp>
#include <Tracy/Tracy.hpp>

#include <fstream>
#include <vector>

namespace fs
{
    inline result<bytes> read_file(const fs::path& path)
    {
        ZoneScoped;
        std::ifstream file(path.c_str(), std::ios::binary | std::ios::ate);
        if (!file)
        {
            return "failed to open file";
        }

        const auto size = file.tellg();
        bytes data(size);
        file.seekg(0, std::ios::beg);
        file.read(data.get<char>(), size);

        return data;
    }
}
