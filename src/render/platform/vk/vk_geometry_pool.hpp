#pragma once

#include <types.hpp>

#include <render/platform/vk/vk_buffer.hpp>
#include <render/platform/vk/vk_error.hpp>
#include <render/platform/vk/vk_renderer.hpp>

namespace render
{
    struct vk_shared_buffer
    {
        u64 size;
        u64 offset;
        vk_buffer buffer;

        explicit vk_shared_buffer(const render::vk_renderer& renderer, const u64 size, VkBufferUsageFlags usage)
            : size(size)
            , offset(0)
        {
            const VkBufferCreateInfo buffer_info {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size  = size,
                .usage = usage,
            };

            VK_ASSERT_ON_FAIL(render::create_buffer(buffer_info,
                                                    renderer.get_context().allocator,
                                                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                                    &buffer))
        }
    };

    struct vk_scene_geometry_pool
    {
        vk_shared_buffer vertex;
        vk_shared_buffer index;
    };
}
