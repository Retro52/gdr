#pragma once

#include <volk.h>

#include <types.hpp>

#include <render/platform/vk/vma.hpp>
#include <result.hpp>

namespace render
{
    struct vk_buffer
    {
        VkBuffer buffer {VK_NULL_HANDLE};
        VmaAllocation allocation {VK_NULL_HANDLE};
    };

    void destroy_buffer(VmaAllocator allocator, const vk_buffer& buffer);

    result<vk_buffer> create_buffer(u64 size, VkBufferUsageFlags usage, VmaAllocator allocator,
                                    VmaAllocationCreateFlags allocation_flags);

    result<vk_buffer> create_buffer(const VkBufferCreateInfo& buffer_create_info, VmaAllocator allocator,
                                    VmaAllocationCreateFlags allocation_flags);

    result<VkBufferView> create_buffer_view(VkDevice device, VkBuffer buffer, VkFormat format, u64 offset, u64 range);
}
