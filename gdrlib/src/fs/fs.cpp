#include <SDL3/SDL_iostream.h>

#include <fs/fs.hpp>
#include <janitor.hpp>
#include <tracy/Tracy.hpp>

#include <filesystem>

bool fs::exists(const fs::path& path)
{
    return std::filesystem::exists(path.c_str());
}

result<bytes> fs::read_file(const fs::path& path)
{
    ZoneScoped;
    const auto io = SDL_IOFromFile(path.c_str(), "rb");
    if (!io)
    {
        return error("failed to open file");
    }

    SUMMON_JANITOR(SDL_CloseIO(io));

    const auto size = SDL_GetIOSize(io);
    bytes data(size);

    SDL_ReadIO(io, data.get<char>(), size);
    return data;
}

void fs::write_file(const fs::path& path, const bytes& data)
{
    ZoneScoped;

    if (!std::filesystem::exists(path.parent().c_str()))
    {
        std::filesystem::create_directories(path.parent().c_str());
    }

    const auto io = SDL_IOFromFile(path.c_str(), "wb");
    if (!io)
    {
        return;
    }

    SUMMON_JANITOR(SDL_CloseIO(io));
    SDL_WriteIO(io, data.get<char>(), data.size());
}
