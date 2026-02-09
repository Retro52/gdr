#include <volk.h>

#include <render/platform/vk/vk_error.hpp>
#include <render/platform/vk/vk_image.hpp>

VkImageSubresourceRange render::image_subresource_range(VkImageAspectFlags aspect_flag)
{
    return render::image_subresource_range(aspect_flag, 0, VK_REMAINING_MIP_LEVELS);
}

VkImageSubresourceRange render::image_subresource_range(VkImageAspectFlags aspect_flag, u32 mip_level, u32 levels_count)
{
    return VkImageSubresourceRange {
        .aspectMask     = aspect_flag,
        .baseMipLevel   = mip_level,
        .levelCount     = levels_count,
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

    return transition_image(cmd, image, current_layout, new_layout, aspect_flag);
}

void render::transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout,
                              VkImageLayout new_layout, VkImageAspectFlags aspect_flags)
{
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
        .subresourceRange = image_subresource_range(aspect_flags),
    };

    const VkDependencyInfo dependency_info {
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                   = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &image_barrier,
    };

    vkCmdPipelineBarrier2(cmd, &dependency_info);
}

void render::destroy_image(VkDevice device, VmaAllocator allocator, const vk_image& image)
{
    vkDestroyImageView(device, image.view, nullptr);
    vmaDestroyImage(allocator, image.image, image.allocation);
}

result<render::vk_image> render::create_image(VkDevice device, const VkImageCreateInfo& image_create_info,
                                              VkImageAspectFlags aspect_flags, VmaAllocator allocator)
{
    vk_image image;
    constexpr VmaAllocationCreateInfo alloc_info = {.usage = VMA_MEMORY_USAGE_AUTO};
    VK_RETURN_ON_FAIL(
        vmaCreateImage(allocator, &image_create_info, &alloc_info, &image.image, &image.allocation, nullptr));
    image.view = *render::create_image_view(device, image.image, image_create_info.format, aspect_flags);

    return image;
}

result<VkImageView> render::create_image_view(VkDevice device, VkImage image, VkFormat format,
                                              VkImageAspectFlags aspect_flags)
{
    return render::create_image_view(device, image, format, aspect_flags, 0, VK_REMAINING_MIP_LEVELS);
}

result<VkImageView> render::create_image_view(VkDevice device, VkImage image, VkFormat format,
                                              VkImageAspectFlags aspect_flags, u32 mip_level, u32 levels_count)
{
    const VkImageViewCreateInfo image_view_create_info {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext            = nullptr,
        .image            = image,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = format,
        .subresourceRange = image_subresource_range(aspect_flags, mip_level, levels_count)};

    VkImageView view;
    VK_RETURN_ON_FAIL(vkCreateImageView(device, &image_view_create_info, nullptr, &view));

    return view;
}

result<VkSampler> render::create_sampler(VkDevice device, VkFilter filter, VkSamplerMipmapMode mipmap_mode,
                                         VkSamplerAddressMode sampler_address_mode,
                                         VkSamplerReductionMode reduction_mode)
{
    VkSamplerCreateInfo sampler_info {
        .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter    = filter,
        .minFilter    = filter,
        .mipmapMode   = mipmap_mode,
        .addressModeU = sampler_address_mode,
        .addressModeV = sampler_address_mode,
        .addressModeW = sampler_address_mode,
        .minLod       = 0.0F,
        .maxLod       = 16.0F,
    };

    VkSamplerReductionModeCreateInfo reduction_mode_info {VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO};
    if (reduction_mode != VK_SAMPLER_REDUCTION_MODE_MAX_ENUM)
    {
        assert2m((reduction_mode != VK_SAMPLER_REDUCTION_MODE_MIN && reduction_mode != VK_SAMPLER_REDUCTION_MODE_MAX)
                     || filter != VK_FILTER_NEAREST,
                 "Using min/max reduction with nearest filter is incorrect");

        sampler_info.pNext                = &reduction_mode_info;
        reduction_mode_info.reductionMode = reduction_mode;
    }

    VkSampler sampler;
    VK_RETURN_ON_FAIL(vkCreateSampler(device, &sampler_info, nullptr, &sampler));

    return sampler;
}
