#pragma once

#include <types.hpp>

#include <render/platform/vk/vma.hpp>
#include <result.hpp>

namespace render
{
    struct vk_buffer
    {
        u64 size {0};
        VkBuffer buffer {VK_NULL_HANDLE};
        VmaAllocation allocation {VK_NULL_HANDLE};
    };

    struct vk_mapped_buffer
    {
        u64 size {0};
        void* mapped {nullptr};
        VkBuffer buffer {VK_NULL_HANDLE};
        VmaAllocation allocation {VK_NULL_HANDLE};
    };

    void destroy_buffer(VmaAllocator allocator, vk_buffer& buffer);

    result<vk_buffer> create_buffer(u64 size, VkBufferUsageFlags usage, VmaAllocator allocator,
                                    VmaAllocationCreateFlags allocation_flags);

    result<vk_buffer> create_buffer(const VkBufferCreateInfo& buffer_create_info, VmaAllocator allocator,
                                    VmaAllocationCreateFlags allocation_flags);

    result<vk_mapped_buffer> create_buffer_mapped(u64 size, VkBufferUsageFlags usage, VmaAllocator allocator,
                                    VmaAllocationCreateFlags allocation_flags);

    result<vk_mapped_buffer> create_buffer_mapped(const VkBufferCreateInfo& buffer_create_info, VmaAllocator allocator,
                                VmaAllocationCreateFlags allocation_flags);

    result<VkBufferView> create_buffer_view(VkDevice device, VkBuffer buffer, VkFormat format, u64 offset, u64 range);
}
