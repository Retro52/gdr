#pragma once

#include <types.hpp>

#include <render/platform/vk/vma.hpp>
#include <volk.h>

namespace render
{
    struct vk_buffer
    {
        VkBuffer buffer {VK_NULL_HANDLE};
        VmaAllocation allocation {VK_NULL_HANDLE};
    };

    void destroy_buffer(VmaAllocator allocator, const vk_buffer& buffer);

    VkResult create_buffer(const VkBufferCreateInfo& buffer_create_info, VmaAllocator allocator,
                           VmaAllocationCreateFlags allocation_flags, vk_buffer* buffer);

    VkResult create_buffer_view(VkDevice device, VkBuffer buffer, VkFormat format, u64 offset, u64 range,
                                VkBufferView* view);
}
