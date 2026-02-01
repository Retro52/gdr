#pragma once

#include <types.hpp>

#include <bytes.hpp>
#include <fs/path.hpp>
#include <result.hpp>
#include <tracy/Tracy.hpp>

#include <fstream>

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

    inline void write_file(const fs::path& path, const bytes& data)
    {
        ZoneScoped;

        if (!std::filesystem::exists(path.parent().c_str()))
        {
            std::filesystem::create_directories(path.parent().c_str());
        }

        std::ofstream file(path.c_str(), std::ios::binary);
        if (!file)
        {
            return;
        }

        file.write(data.get<char>(), static_cast<std::streamsize>(data.size()));
    }
}
