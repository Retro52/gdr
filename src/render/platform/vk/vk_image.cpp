#include <volk.h>

#include <render/platform/vk/vk_error.hpp>
#include <render/platform/vk/vk_image.hpp>

VkImageSubresourceRange render::image_subresource_range(VkImageAspectFlags aspect_flag)
{
    return VkImageSubresourceRange {
        .aspectMask     = aspect_flag,
        .baseMipLevel   = 0,
        .levelCount     = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    };
}

void render::transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout,
                              VkImageLayout new_layout)
{
    const VkImageAspectFlags aspect_flag = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
                                             ? VK_IMAGE_ASPECT_DEPTH_BIT
                                             : VK_IMAGE_ASPECT_COLOR_BIT;

    const VkImageMemoryBarrier2 image_barrier {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext            = nullptr,
        .srcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
        .oldLayout        = current_layout,
        .newLayout        = new_layout,
        .image            = image,
        .subresourceRange = image_subresource_range(aspect_flag),
    };

    const VkDependencyInfo dependency_info {
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                   = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &image_barrier,
    };

    vkCmdPipelineBarrier2(cmd, &dependency_info);
}

void render::destroy_image(VmaAllocator allocator, const vk_image& image)
{
    vmaDestroyImage(allocator, image.image, image.allocation);
}

result<render::vk_image> render::create_image(const VkImageCreateInfo& image_create_info, VmaAllocator allocator)
{
    vk_image image;
    constexpr VmaAllocationCreateInfo alloc_info = {.usage = VMA_MEMORY_USAGE_AUTO};
    VK_RETURN_ON_FAIL(
        vmaCreateImage(allocator, &image_create_info, &alloc_info, &image.image, &image.allocation, nullptr));

    return image;
}

result<VkImageView> render::create_image_view(VkDevice device, VkImage image, VkFormat format,
                                              VkImageAspectFlags aspect_flags)
{
    const VkImageViewCreateInfo image_view_create_info {.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                                        .pNext            = nullptr,
                                                        .image            = image,
                                                        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
                                                        .format           = format,
                                                        .subresourceRange = image_subresource_range(aspect_flags)};

    VkImageView view;
    VK_RETURN_ON_FAIL(vkCreateImageView(device, &image_view_create_info, nullptr, &view));

    return view;
}
