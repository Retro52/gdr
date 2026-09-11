#pragma once

#include <bytes.hpp>
#include <fs/path.hpp>
#include <result.hpp>

namespace fs
{
    bool exists(const fs::path& path);

    result<bytes> read_file(const fs::path& path);

    void write_file(const fs::path& path, const bytes& data);
}
