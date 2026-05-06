#pragma once

#include <pod_types.hpp>
#include <render/platform/vk/vk_buffer.hpp>
#include <render/platform/vk/vk_image.hpp>
#include <render/platform/vk/vk_pipeline.hpp>

namespace app
{
    struct depth_pyramid_data
    {
        VkImageView views[12] {};
        render::vk_image image;
        VkSampler sampler;
        ivec2 base_size;
        u32 pyramid_count {0};
    };

    void begin_rendering(VkCommandBuffer cmd, VkImageView color, VkImageView depth, VkAttachmentLoadOp load_op,
                         VkAttachmentStoreOp store_op, const VkRect2D& vp);

    void reset_draw_count_buffer(VkCommandBuffer cmd, const render::vk_buffer& draw_count_buffer);

    render::vk_image create_depth_image(const ivec2& size, VkFormat format, VkDevice device, VmaAllocator allocator);

    void destroy_depth_pyramid(depth_pyramid_data& pyramid, VkDevice device, VmaAllocator allocator);

    depth_pyramid_data create_depth_pyramid(const ivec2& size, VkFormat format, VkDevice device,
                                            VmaAllocator allocator);
}
