#pragma once

#include <volk.h>

#include <pod_types.hpp>
#include <render/rhi.hpp>

namespace platform
{
    enum class color_space
    {
        linear,
        srgb
    };

    VkFormat vk_format_from_dxgi(u32 dx_format);
    VkFormat vk_format_force_color_space(VkFormat vk_format, color_space space);

    VkImageLayout vk_rhi_parse_image_layout(rhi::image_layout layout);
    VkImageViewType vk_rhi_parse_image_view_type(rhi::image_view_kind kind);

    VkImageAspectFlags vk_rhi_parse_aspect_flags(rhi::image_aspect_bits aspects);
    VkImageAspectFlags vk_rhi_parse_format_aspect_flags(VkFormat format);

    rhi::shader_stage vk_rhi_translate_shader_stage(VkShaderStageFlagBits stage);

    VkImageUsageFlags vk_rhi_parse_image_usage_flags(rhi::image_usage_bits usage);
    VkBufferUsageFlags vk_rhi_parse_buffer_usage_flags(rhi::buffer_usage_bits usage);

    VkFilter vk_rhi_parse_sampler_filter(rhi::sampler_filter sampler_filter);
    VkBorderColor vk_rhi_parse_sampler_border_color(rhi::sampler_border_color sampler_border_color);
    VkSamplerReductionMode vk_rhi_parse_sampler_reduction(rhi::sampler_reduction sampler_reduction);
    VkSamplerMipmapMode vk_rhi_parse_sampler_mipmap_mode(rhi::sampler_mipmap_mode sampler_mipmap_mode);
    VkSamplerAddressMode vk_rhi_parse_sampler_address_mode(rhi::sampler_address_mode sampler_address_mode);

    VkCompareOp vk_rhi_parse_compare_op(rhi::compare_op compare_op);

    VkClearValue vk_rhi_parse_color_clear_value(rhi::clear_value cv);
    VkClearValue vk_rhi_parse_depth_clear_value(rhi::clear_value cv);

    VkPrimitiveTopology vk_rhi_parse_topology(rhi::topology topology);

    VkBlendOp vk_rhi_parse_blend_op(rhi::blend_op blend_op);
    VkBlendFactor vk_rhi_parse_blend_factor(rhi::blend_factor blend_factor);

    VkAttachmentLoadOp vk_rhi_parse_load_op(rhi::resource_load_op load_op);
    VkAttachmentStoreOp vk_rhi_parse_store_op(rhi::resource_store_op store_op);

    VkAccessFlags2 vk_rhi_parse_access_flags(rhi::barrier_access_bits access);
    VkPipelineStageFlags2 vk_rhi_parse_stage_flags(rhi::barrier_stage_bits stages);
}
