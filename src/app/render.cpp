#include <app/render.hpp>
#include <render/platform/vk/vk_barrier.hpp>
#include <tracy/Tracy.hpp>

#include <cmath>

void app::begin_rendering(VkCommandBuffer cmd, VkImageView color, VkImageView depth, VkAttachmentLoadOp load_op,
                          VkAttachmentStoreOp store_op, const VkRect2D& vp)
{
    ZoneScoped;
    VkRenderingInfo rendering_info {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR, .renderArea = vp, .layerCount = 1};

    VkRenderingAttachmentInfo color_attachment_info {};
    VkRenderingAttachmentInfo depth_attachment_info {};
    if (color != VK_NULL_HANDLE)
    {
        color_attachment_info = {
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView   = color,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            .loadOp      = load_op,
            .storeOp     = store_op,
        };

        cpp::cx_fill(
            &color_attachment_info.clearValue.color.uint32[0], &color_attachment_info.clearValue.color.uint32[4], ~0U);

        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments    = &color_attachment_info;
    }

    if (depth != VK_NULL_HANDLE)
    {
        depth_attachment_info = {
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView   = depth,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            .loadOp      = load_op,
            .storeOp     = store_op,
            .clearValue  = {
                            .depthStencil = {0.0F, 0},
                            }
        };

        rendering_info.pDepthAttachment = &depth_attachment_info;
    }

    vkCmdBeginRendering(cmd, &rendering_info);
}

void app::zero_buffer(VkCommandBuffer cmd, const render::vk_buffer& buffer, u64 offset, u64 size)
{
    ZoneScoped;

#ifdef __APPLE__
    constexpr auto stage_bits = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
#else
    constexpr auto stage_bits = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT
                              | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
#endif

    render::cmd_buffer_barrier(cmd,
                               buffer.buffer,
                               stage_bits,
                               VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT
                                   | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                               VK_PIPELINE_STAGE_2_CLEAR_BIT,
                               VK_ACCESS_2_TRANSFER_WRITE_BIT);

    vkCmdFillBuffer(cmd, buffer.buffer, offset, size ? size : (buffer.size - offset), 0);

    render::cmd_buffer_barrier(cmd,
                               buffer.buffer,
                               VK_PIPELINE_STAGE_2_CLEAR_BIT,
                               VK_ACCESS_2_TRANSFER_WRITE_BIT,
                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                               VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
}

void app::reset_draw_count_buffer(VkCommandBuffer cmd, const render::vk_buffer& draw_count_buffer)
{
    ZoneScoped;

#ifdef __APPLE__
    constexpr auto stage_bits = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
#else
    constexpr auto stage_bits = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT
                              | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
#endif

    render::cmd_buffer_barrier(cmd,
                               draw_count_buffer.buffer,
                               stage_bits,
                               VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT
                                   | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                               VK_PIPELINE_STAGE_2_CLEAR_BIT,
                               VK_ACCESS_2_TRANSFER_WRITE_BIT);

    vkCmdFillBuffer(cmd, draw_count_buffer.buffer, 0, sizeof(u32), 0);
    vkCmdFillBuffer(cmd, draw_count_buffer.buffer, sizeof(u32), sizeof(u32[2]), 1);

    render::cmd_buffer_barrier(cmd,
                               draw_count_buffer.buffer,
                               VK_PIPELINE_STAGE_2_CLEAR_BIT,
                               VK_ACCESS_2_TRANSFER_WRITE_BIT,
                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                               VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
}

render::vk_image app::create_depth_image(const ivec2& size, const VkFormat format, VkDevice device,
                                         VmaAllocator allocator)
{
    ZoneScoped;
    const VkImageCreateInfo image_create_info {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = format,
        .extent        = {static_cast<u32>(size.x), static_cast<u32>(size.y), 1},
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    return *render::create_image(device, image_create_info, VK_IMAGE_ASPECT_DEPTH_BIT, allocator);
}

void app::destroy_depth_pyramid(depth_pyramid_data& pyramid, VkDevice device, VmaAllocator allocator)
{
    ZoneScoped;
    for (u32 i = 0; i < pyramid.pyramid_count; ++i)
    {
        vkDestroyImageView(device, pyramid.views[i], nullptr);
    }

    pyramid.pyramid_count = 0;
    render::destroy_image(device, allocator, pyramid.image);
    vkDestroySampler(device, pyramid.sampler, nullptr);
}

app::vis_buffer_data app::create_vis_buffer_data(const ivec2& size, VkDevice device, VmaAllocator allocator)
{
    ZoneScoped;

    const VkImageCreateInfo image_create_info {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = VK_FORMAT_R32G32_UINT,
        .extent      = {static_cast<u32>(size.x), static_cast<u32>(size.y), 1},
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .usage       = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
               | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    return {*render::create_image(device, image_create_info, VK_IMAGE_ASPECT_COLOR_BIT, allocator)};
}

app::depth_pyramid_data app::create_depth_pyramid(const ivec2& size, const VkFormat format, VkDevice device,
                                                  VmaAllocator allocator)
{
    ZoneScoped;
    depth_pyramid_data depth_pyramid {
        .base_size     = {1 << static_cast<i32>(std::log2(size.x)), 1 << static_cast<i32>(std::log2(size.y))},
        .pyramid_count = 1,
    };

    ivec2 size_cpy = depth_pyramid.base_size;
    while (size_cpy.x > 1 || size_cpy.y > 1)
    {
        size_cpy /= 2;
        ++depth_pyramid.pyramid_count;
    }

    depth_pyramid.pyramid_count =
        std::min(depth_pyramid.pyramid_count, static_cast<u32>(COUNT_OF(depth_pyramid.views)));

    const VkImageCreateInfo image_create_info {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = format,
        .extent        = {static_cast<u32>(depth_pyramid.base_size.x), static_cast<u32>(depth_pyramid.base_size.y), 1},
        .mipLevels     = depth_pyramid.pyramid_count,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    depth_pyramid.image   = *render::create_image(device, image_create_info, VK_IMAGE_ASPECT_COLOR_BIT, allocator);
    depth_pyramid.sampler = *render::create_sampler(device,
                                                    VK_FILTER_LINEAR,
                                                    VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                                    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
#ifdef __APPLE__
                                                    VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE);
#else
                                                    VK_SAMPLER_REDUCTION_MODE_MIN);
#endif

    for (u32 i = 0; i < depth_pyramid.pyramid_count; ++i)
    {
        depth_pyramid.views[i] = *render::create_image_view(
            device, depth_pyramid.image.image, VK_IMAGE_VIEW_TYPE_2D, format, VK_IMAGE_ASPECT_COLOR_BIT, i, 1);
    }

    return depth_pyramid;
}
