#pragma once

#include <volk.h>

#include <pod_types.hpp>
#include <render/rhi.hpp>

namespace render
{
    enum class color_space
    {
        linear,
        srgb
    };

    VkFormat vk_format_from_dxgi(u32 dx_format);
    VkFormat vk_format_force_color_space(VkFormat vk_format, color_space space);

    VkImageViewType vk_rhi_parse_image_view_type(render::rhi::image_view_kind kind);

    VkImageAspectFlags vk_rhi_parse_aspect_flags(render::rhi::image_aspects aspects);
    VkImageAspectFlags vk_rhi_parse_format_aspect_flags(VkFormat format);

    render::rhi::shader_stage vk_rhi_translate_shader_stage(VkShaderStageFlagBits stage);

    VkImageUsageFlags vk_rhi_parse_image_usage_flags(render::rhi::image_usages usage);
    VkBufferUsageFlags vk_rhi_parse_buffer_usage_flags(render::rhi::buffer_usages usage);

    VkFilter vk_rhi_parse_sampler_filter(render::rhi::sampler_filter sampler_filter);
    VkBorderColor vk_rhi_parse_sampler_border_color(render::rhi::sampler_border_color sampler_border_color);
    VkSamplerReductionMode vk_rhi_parse_sampler_reduction(render::rhi::sampler_reduction sampler_reduction);
    VkSamplerMipmapMode vk_rhi_parse_sampler_mipmap_mode(render::rhi::sampler_mipmap_mode sampler_mipmap_mode);
    VkSamplerAddressMode vk_rhi_parse_sampler_address_mode(render::rhi::sampler_address_mode sampler_address_mode);

    VkCompareOp vk_rhi_parse_compare_op(render::rhi::compare_op compare_op);

    VkClearValue vk_rhi_parse_color_clear_value(render::rhi::clear_value cv);
    VkClearValue vk_rhi_parse_depth_clear_value(render::rhi::clear_value cv);

    VkPrimitiveTopology vk_rhi_parse_topology(render::rhi::topology topology);

    VkBlendOp vk_rhi_parse_blend_op(render::rhi::blend_op blend_op);
    VkBlendFactor vk_rhi_parse_blend_factor(render::rhi::blend_factor blend_factor);

}
