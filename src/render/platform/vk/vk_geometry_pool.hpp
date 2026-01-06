#pragma once

#include <types.hpp>

#include <render/platform/vk/vk_buffer.hpp>
#include <render/platform/vk/vk_buffer_transfer.hpp>
#include <render/platform/vk/vk_renderer.hpp>

namespace render
{
    struct vk_shared_buffer
    {
        u64 size;
        u64 offset;
        vk_buffer buffer;

        vk_shared_buffer() = default;

        explicit vk_shared_buffer(const render::vk_renderer& renderer, const u64 size, VkBufferUsageFlags usage)
            : size(size)
            , offset(0)
        {
            buffer = *render::create_buffer(
                size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, renderer.get_context().allocator, 0);
        }
    };

    struct vk_scene_geometry_pool
    {
        vk_shared_buffer index;
        vk_shared_buffer vertex;
        vk_shared_buffer meshlets;

        vk_shared_buffer meshlets_indices;
        vk_shared_buffer meshlets_vertices;

        vk_buffer_transfer transfer;
    };
}
