#pragma once

#include <volk.h>

#include <pod_types.hpp>
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

    struct vk_shared_buffer
    {
        u64 size {0};
        u64 offset {0};
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

    result<vk_buffer_transfer> create_buffer_transfer(VkDevice device, VmaAllocator allocator, const queue_data& queue,
                                                      u64 staging_memory_size);

    void destroy_buffer_transfer(VkDevice device, VmaAllocator allocator, vk_buffer_transfer& buffer_transfer);

    void submit_transfer(const vk_buffer_transfer& transfer, const vk_buffer& dst, const VkBufferCopy& region);

    void upload_data(const vk_buffer_transfer& transfer, const vk_buffer& dst, const u8* data,
                     const VkBufferCopy& region);

    void upload_image(const vk_buffer_transfer& transfer, const vk_image& dst, const u8* data, u64 data_size, u32 width,
                      u32 height, u32 mips, u32 block_size, u32 bits_per_block);

    void fill_buffer(const vk_buffer_transfer& transfer, const vk_buffer& dst, const u8* value_ptr, u64 value_size,
                     const VkBufferCopy& region);

    template<typename T>
    void upload_data(const vk_buffer_transfer& transfer, const vk_buffer& dst, const T* data, const u64 count)
    {
        ZoneScoped;
        upload_data(transfer, dst, reinterpret_cast<const u8*>(data), VkBufferCopy {.size = count * sizeof(T)});
    }

    template<typename T>
    void upload_data(const render::vk_buffer_transfer& transfer, render::vk_shared_buffer& dst_buffer, const T* data,
                     const u64 count)
    {
        ZoneScoped;
        render::upload_data(transfer,
                            dst_buffer.buffer,
                            reinterpret_cast<const u8*>(data),
                            VkBufferCopy {.srcOffset = 0, .dstOffset = dst_buffer.offset, .size = count * sizeof(T)});
        dst_buffer.offset += count * sizeof(T);
    }

    template<typename T>
    void fill_buffer(const vk_buffer_transfer& transfer, const vk_buffer& dst, T&& value)
    {
        ZoneScoped;
        fill_buffer(transfer, dst, reinterpret_cast<const u8*>(&value), sizeof(T), VkBufferCopy {.size = dst.size});
    }
}
