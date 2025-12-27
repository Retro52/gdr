#pragma once

#include <types.hpp>

#include <render/platform/vk/vk_buffer.hpp>

namespace render
{
    struct vk_index_geometry_pool
    {
        u64 size;
        u64 offset;
        vk_buffer buffer;
    };

    struct vk_vertex_geometry_pool
    {
        u64 size;
        u64 offset;
        vk_buffer buffer;
    };

    struct vk_scene_geometry_pool
    {
        vk_vertex_geometry_pool vertex;
        vk_index_geometry_pool index;
    };
}
