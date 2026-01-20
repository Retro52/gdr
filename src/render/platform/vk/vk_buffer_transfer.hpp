#pragma once

#include <volk.h>

#include <types.hpp>

#include <render/platform/vk/vk_buffer.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <result.hpp>

namespace render
{
    struct vk_buffer_transfer
    {
        void* mapped;
        queue_data queue;
        vk_buffer staging_buffer;
        vk_command_buffer staging_command_buffer;
    };

    result<vk_buffer_transfer> create_buffer_transfer(VkDevice device, VmaAllocator allocator, const queue_data& queue,
                                                      u64 staging_memory_size);

    void destroy_buffer_transfer(VkDevice device, VmaAllocator allocator, vk_buffer_transfer& buffer_transfer);

    void upload_data(const vk_buffer_transfer& transfer, const vk_buffer& dst, const u8* data,
                     const VkBufferCopy& region);

    template<typename T>
    void upload_data(const vk_buffer_transfer& transfer, const vk_buffer& dst, const T* data, const u64 count)
    {
        upload_data(transfer, dst, reinterpret_cast<const u8*>(data), VkBufferCopy {.size = count * sizeof(T)});
    }
}
