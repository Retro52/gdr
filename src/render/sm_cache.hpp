#pragma once

#include <types.hpp>

#include <fs/path.hpp>
#include <result.hpp>

#include <vector>

namespace render
{
    template<typename V>
    struct mesh_data
    {
        std::vector<V> vertices;
        std::vector<u32> indices;
    };

    template<typename V>
    result<std::vector<mesh_data<V>>> parse_model(const fs::path& path);

    template<typename V>
    bool load_model(const fs::path& path, std::vector<mesh_data<V>>& meshes);
}
