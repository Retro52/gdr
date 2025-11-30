#pragma once

#include <render/platform/vk/vma.hpp>
#include <volk.h>

namespace render
{
    struct vk_image
    {
        VkImage image {VK_NULL_HANDLE};
        VmaAllocation allocation {VK_NULL_HANDLE};
    };

    VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspect_flag);

    void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout, VkImageLayout new_layout);

    void destroy_image(VmaAllocator allocator, const vk_image& image);

    VkResult create_image(const VkImageCreateInfo& image_create_info, VmaAllocator allocator, vk_image* image);

    VkResult create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags,
                               VkImageView* view);
}
