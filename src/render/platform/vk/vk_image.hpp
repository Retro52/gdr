#pragma once

#include <render/platform/vk/vma.hpp>
#include <result.hpp>

namespace render
{
    struct vk_image
    {
        VkImage image {VK_NULL_HANDLE};
        VmaAllocation allocation {VK_NULL_HANDLE};
    };

    VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspect_flag);

    VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspect_flag, u32 mip_level, u32 levels_count);

    void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout, VkImageLayout new_layout);

    void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout, VkImageLayout new_layout, VkImageAspectFlags aspect_flags);

    void destroy_image(VmaAllocator allocator, const vk_image& image);

    result<vk_image> create_image(const VkImageCreateInfo& image_create_info, VmaAllocator allocator);

    result<VkImageView> create_image_view(VkDevice device, VkImage image, VkFormat format,
                                          VkImageAspectFlags aspect_flags);

    result<VkImageView> create_image_view(VkDevice device, VkImage image, VkFormat format,
                                      VkImageAspectFlags aspect_flags, u32 mip_level, u32 levels_count);
}
