#pragma once

#include <fs/fs.hpp>
#include <tracy/Tracy.hpp>

#include <fstream>

result<bytes> fs::read_file(const fs::path& path)
{
    ZoneScoped;
    std::ifstream file(path.c_str(), std::ios::binary | std::ios::ate);
    if (!file)
    {
        if (file.bad())
        {
            return error("fs::read_file: badbit is set.");
        }

        return error("fs::read_file: failed to open file");
    }

    const auto size = file.tellg();
    bytes data(size);
    file.seekg(0, std::ios::beg);
    file.read(data.get<char>(), size);

    return data;
}

void fs::write_file(const fs::path& path, const bytes& data)
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
