#pragma once

#include <volk.h>

#include <render/platform/d3d12/d3d12.hpp>
#include <render/rhi.hpp>

namespace platform
{
    DXGI_FORMAT d3d12_format_from_vk(VkFormat vk_format);

    D3D12_RESOURCE_FLAGS d3d12_rhi_parse_image_usage(rhi::image_usage_bits usage);

    uvec2 d3d12_rhi_parse_aspect_flags(rhi::image_aspect_bits aspects);
    D3D12_BARRIER_LAYOUT d3d12_rhi_parse_image_layout(rhi::image_layout layout);
    D3D12_BARRIER_SUBRESOURCE_RANGE d3d12_rhi_parse_image_range(const rhi::image_subresource_range& range);

    D3D_PRIMITIVE_TOPOLOGY d3d12_rhi_parse_topology(rhi::topology topology);
    D3D12_COMPARISON_FUNC d3d12_rhi_parse_comparison_func(rhi::compare_op op);
    D3D12_BARRIER_SYNC d3d12_rhi_parse_stage_flags(rhi::barrier_stage_bits stages);
    D3D12_BARRIER_ACCESS d3d12_rhi_parse_access_flags(rhi::barrier_access_bits access);

    D3D12_FILTER_TYPE d3d12_rhi_parse_filter(rhi::sampler_filter filter);
    D3D12_FILTER_TYPE d3d12_rhi_parse_mipmap(rhi::sampler_mipmap_mode mode);
    D3D12_FILTER_REDUCTION_TYPE d3d12_rhi_parse_reduction(rhi::sampler_reduction reduction);
    D3D12_TEXTURE_ADDRESS_MODE d3d12_rhi_parse_address_mode(rhi::sampler_address_mode address_mode);
    D3D12_FILTER d3d12_rhi_parse_filter_type(rhi::sampler_filter filter, rhi::sampler_reduction reduction,
                                             rhi::sampler_mipmap_mode mipmap_mode);
}
