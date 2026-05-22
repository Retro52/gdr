#pragma once

#include <render/platform/vk/vk_buffer.hpp>
#include <render/platform/vk/vk_buffer_transfer.hpp>
#include <render/platform/vk/vk_renderer.hpp>

namespace render
{
    struct vk_scene_geometry_pool
    {
        vk_shared_buffer index;
        vk_shared_buffer vertex;
        vk_shared_buffer meshlets;
        vk_shared_buffer primitives;
        vk_shared_buffer instances;
        vk_shared_buffer materials;
        vk_shared_buffer meshlets_payload;

        vk_buffer_transfer transfer;
    };
}
