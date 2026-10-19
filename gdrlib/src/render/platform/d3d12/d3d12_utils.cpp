#pragma once

#include <ddspp.h>
#include <render/platform/d3d12/d3d12_utils.hpp>
#include <tracy/Tracy.hpp>

static u32 ddspp_format_from_vk(const VkFormat vk_format)
{
    ZoneScoped;
    using ddspp::DXGIFormat;

    switch (vk_format)
    {
    case VK_FORMAT_R32G32B32A32_SFLOAT :
        return ddspp::R32G32B32A32_FLOAT;
    case VK_FORMAT_R32G32B32A32_UINT :
        return ddspp::R32G32B32A32_UINT;
    case VK_FORMAT_R32G32B32A32_SINT :
        return ddspp::R32G32B32A32_SINT;
    case VK_FORMAT_R32G32B32_SFLOAT :
        return ddspp::R32G32B32_FLOAT;
    case VK_FORMAT_R32G32B32_UINT :
        return ddspp::R32G32B32_UINT;
    case VK_FORMAT_R32G32B32_SINT :
        return ddspp::R32G32B32_SINT;
    case VK_FORMAT_R16G16B16A16_SFLOAT :
        return ddspp::R16G16B16A16_FLOAT;
    case VK_FORMAT_R16G16B16A16_UNORM :
        return ddspp::R16G16B16A16_UNORM;
    case VK_FORMAT_R16G16B16A16_UINT :
        return ddspp::R16G16B16A16_UINT;
    case VK_FORMAT_R16G16B16A16_SNORM :
        return ddspp::R16G16B16A16_SNORM;
    case VK_FORMAT_R16G16B16A16_SINT :
        return ddspp::R16G16B16A16_SINT;
    case VK_FORMAT_R32G32_SFLOAT :
        return ddspp::R32G32_FLOAT;
    case VK_FORMAT_R32G32_UINT :
        return ddspp::R32G32_UINT;
    case VK_FORMAT_R32G32_SINT :
        return ddspp::R32G32_SINT;
    case VK_FORMAT_D32_SFLOAT_S8_UINT :
        return ddspp::D32_FLOAT_S8X24_UINT;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32 :
        return ddspp::R10G10B10A2_UNORM;
    case VK_FORMAT_A2B10G10R10_UINT_PACK32 :
        return ddspp::R10G10B10A2_UINT;
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32 :
        return ddspp::R11G11B10_FLOAT;
    case VK_FORMAT_R8G8B8A8_UNORM :
        return ddspp::R8G8B8A8_UNORM;
    case VK_FORMAT_R8G8B8A8_SRGB :
        return ddspp::R8G8B8A8_UNORM_SRGB;
    case VK_FORMAT_R8G8B8A8_UINT :
        return ddspp::R8G8B8A8_UINT;
    case VK_FORMAT_R8G8B8A8_SNORM :
        return ddspp::R8G8B8A8_SNORM;
    case VK_FORMAT_R8G8B8A8_SINT :
        return ddspp::R8G8B8A8_SINT;
    case VK_FORMAT_R16G16_SFLOAT :
        return ddspp::R16G16_FLOAT;
    case VK_FORMAT_R16G16_UNORM :
        return ddspp::R16G16_UNORM;
    case VK_FORMAT_R16G16_UINT :
        return ddspp::R16G16_UINT;
    case VK_FORMAT_R16G16_SNORM :
        return ddspp::R16G16_SNORM;
    case VK_FORMAT_R16G16_SINT :
        return ddspp::R16G16_SINT;
    case VK_FORMAT_D32_SFLOAT :
        return ddspp::D32_FLOAT;
    case VK_FORMAT_R32_SFLOAT :
        return ddspp::R32_FLOAT;
    case VK_FORMAT_R32_UINT :
        return ddspp::R32_UINT;
    case VK_FORMAT_R32_SINT :
        return ddspp::R32_SINT;
    case VK_FORMAT_D24_UNORM_S8_UINT :
        return ddspp::D24_UNORM_S8_UINT;
    case VK_FORMAT_R8G8_UNORM :
        return ddspp::R8G8_UNORM;
    case VK_FORMAT_R8G8_UINT :
        return ddspp::R8G8_UINT;
    case VK_FORMAT_R8G8_SNORM :
        return ddspp::R8G8_SNORM;
    case VK_FORMAT_R8G8_SINT :
        return ddspp::R8G8_SINT;
    case VK_FORMAT_R16_SFLOAT :
        return ddspp::R16_FLOAT;
    case VK_FORMAT_D16_UNORM :
        return ddspp::D16_UNORM;
    case VK_FORMAT_R16_UNORM :
        return ddspp::R16_UNORM;
    case VK_FORMAT_R16_UINT :
        return ddspp::R16_UINT;
    case VK_FORMAT_R16_SNORM :
        return ddspp::R16_SNORM;
    case VK_FORMAT_R16_SINT :
        return ddspp::R16_SINT;
    case VK_FORMAT_R8_UNORM :
        return ddspp::R8_UNORM;
    case VK_FORMAT_R8_UINT :
        return ddspp::R8_UINT;
    case VK_FORMAT_R8_SNORM :
        return ddspp::R8_SNORM;
    case VK_FORMAT_R8_SINT :
        return ddspp::R8_SINT;
    case VK_FORMAT_A8_UNORM :
        return ddspp::A8_UNORM;
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32 :
        return ddspp::R9G9B9E5_SHAREDEXP;
    case VK_FORMAT_B8G8R8G8_422_UNORM :
        return ddspp::G8R8_G8B8_UNORM;
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK :
        return ddspp::BC1_UNORM;
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK :
        return ddspp::BC1_UNORM_SRGB;
    case VK_FORMAT_BC2_UNORM_BLOCK :
        return ddspp::BC2_UNORM;
    case VK_FORMAT_BC2_SRGB_BLOCK :
        return ddspp::BC2_UNORM_SRGB;
    case VK_FORMAT_BC3_UNORM_BLOCK :
        return ddspp::BC3_UNORM;
    case VK_FORMAT_BC3_SRGB_BLOCK :
        return ddspp::BC3_UNORM_SRGB;
    case VK_FORMAT_BC4_UNORM_BLOCK :
        return ddspp::BC4_UNORM;
    case VK_FORMAT_BC4_SNORM_BLOCK :
        return ddspp::BC4_SNORM;
    case VK_FORMAT_BC5_UNORM_BLOCK :
        return ddspp::BC5_UNORM;
    case VK_FORMAT_BC5_SNORM_BLOCK :
        return ddspp::BC5_SNORM;
    case VK_FORMAT_B5G6R5_UNORM_PACK16 :
        return ddspp::B5G6R5_UNORM;
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16 :
        return ddspp::B5G5R5A1_UNORM;
    case VK_FORMAT_B8G8R8A8_UNORM :
        return ddspp::B8G8R8A8_UNORM;
    case VK_FORMAT_B8G8R8A8_SRGB :
        return ddspp::B8G8R8A8_UNORM_SRGB;
    case VK_FORMAT_BC6H_UFLOAT_BLOCK :
        return ddspp::BC6H_UF16;
    case VK_FORMAT_BC6H_SFLOAT_BLOCK :
        return ddspp::BC6H_SF16;
    case VK_FORMAT_BC7_UNORM_BLOCK :
        return ddspp::BC7_UNORM;
    case VK_FORMAT_BC7_SRGB_BLOCK :
        return ddspp::BC7_UNORM_SRGB;
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM :
        return ddspp::NV12;
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 :
        return ddspp::P010;
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM :
        return ddspp::P016;
    case VK_FORMAT_G8B8G8R8_422_UNORM :
        return ddspp::YUY2;
    case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 :
        return ddspp::Y210;
    case VK_FORMAT_G16B16G16R16_422_UNORM :
        return ddspp::Y216;
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16 :
        return ddspp::B4G4R4A4_UNORM;

    // Xbox-specific
    case VK_FORMAT_D16_UNORM_S8_UINT :
        return ddspp::D16_UNORM_S8_UINT;

    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM :
        return ddspp::P208;
    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM :
        return ddspp::V208;
    case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM :
        return ddspp::V408;
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK :
        return ddspp::ASTC_4X4_UNORM;
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK :
        return ddspp::ASTC_4X4_UNORM_SRGB;
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK :
        return ddspp::ASTC_5X4_UNORM;
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK :
        return ddspp::ASTC_5X4_UNORM_SRGB;
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK :
        return ddspp::ASTC_5X5_UNORM;
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK :
        return ddspp::ASTC_5X5_UNORM_SRGB;

    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK :
        return ddspp::ASTC_6X5_UNORM;
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK :
        return ddspp::ASTC_6X5_UNORM_SRGB;
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK :
        return ddspp::ASTC_6X6_UNORM;
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK :
        return ddspp::ASTC_6X6_UNORM_SRGB;
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK :
        return ddspp::ASTC_8X5_UNORM;
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK :
        return ddspp::ASTC_8X5_UNORM_SRGB;
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK :
        return ddspp::ASTC_8X6_UNORM;
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK :
        return ddspp::ASTC_8X6_UNORM_SRGB;
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK :
        return ddspp::ASTC_8X8_UNORM;
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK :
        return ddspp::ASTC_8X8_UNORM_SRGB;
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK :
        return ddspp::ASTC_10X5_UNORM;
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK :
        return ddspp::ASTC_10X5_UNORM_SRGB;
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK :
        return ddspp::ASTC_10X6_UNORM;
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK :
        return ddspp::ASTC_10X6_UNORM_SRGB;
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK :
        return ddspp::ASTC_10X8_UNORM;
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK :
        return ddspp::ASTC_10X8_UNORM_SRGB;
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK :
        return ddspp::ASTC_10X10_UNORM;
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK :
        return ddspp::ASTC_10X10_UNORM_SRGB;
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK :
        return ddspp::ASTC_12X10_UNORM;
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK :
        return ddspp::ASTC_12X10_UNORM_SRGB;
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK :
        return ddspp::ASTC_12X12_UNORM;
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK :
        return ddspp::ASTC_12X12_UNORM_SRGB;

    case VK_FORMAT_UNDEFINED :
    default :
        return ddspp::UNKNOWN;
    }
}

DXGI_FORMAT platform::d3d12_format_from_vk(const VkFormat vk_format)
{
    ZoneScoped;
    return static_cast<DXGI_FORMAT>(ddspp_format_from_vk(vk_format));
}

D3D12_RESOURCE_FLAGS platform::d3d12_rhi_parse_image_usage(const rhi::image_usage_bits usage)
{
    ZoneScoped;
    D3D12_RESOURCE_FLAGS result = D3D12_RESOURCE_FLAG_NONE;
    for (u32 i = 0; i < reflection::get_enum_values_count<rhi::image_usage>(); i++)
    {
        const auto flag = reflection::get_enum_value_at<rhi::image_usage>(i);
        if (!(flag & usage))
        {
            continue;
        }

        switch (flag)
        {
        case rhi::image_usage::storage :
            result |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            break;
        case rhi::image_usage::attachment_color :
            result |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            break;
        case rhi::image_usage::attachment_ds :
            result |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            break;
        default :
            break;
        }
    }

    return result;
}

uvec2 platform::d3d12_rhi_parse_aspect_flags(const rhi::image_aspect_bits aspects)
{
    ZoneScoped;
    assert2(!(aspects & rhi::image_aspect::format));

    const bool color   = aspects & rhi::image_aspect::color;
    const bool depth   = aspects & rhi::image_aspect::depth;
    const bool stencil = aspects & rhi::image_aspect::stencil;

    assert2(!(color && (depth || stencil)));

    constexpr u32 kStencilPlane    = 1;
    constexpr u32 kColorDepthPlane = 0;

    if (depth && stencil)
    {
        return {kColorDepthPlane, 2};
    }
    if (stencil)
    {
        return {kStencilPlane, 1};
    }

    return {kColorDepthPlane, 1};
}

D3D12_BARRIER_LAYOUT platform::d3d12_rhi_parse_image_layout(const rhi::image_layout layout)
{
    ZoneScoped;
    switch (layout)
    {
    case rhi::image_layout::discard :
        return D3D12_BARRIER_LAYOUT_UNDEFINED;
    case rhi::image_layout::present :
        return D3D12_BARRIER_LAYOUT_PRESENT;
    case rhi::image_layout::render_target_color :
        return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
    case rhi::image_layout::render_target_depth_stencil :
        return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
    default :
    case rhi::image_layout::common :
        return D3D12_BARRIER_LAYOUT_COMMON;
    }
}

D3D12_BARRIER_SUBRESOURCE_RANGE platform::d3d12_rhi_parse_image_range(const rhi::image_subresource_range& range)
{
    ZoneScoped;
    const uvec2 planes = d3d12_rhi_parse_aspect_flags(range.aspects);
    return D3D12_BARRIER_SUBRESOURCE_RANGE {
        .IndexOrFirstMipLevel = range.mips_range.x,
        .NumMipLevels         = 1,
        .FirstArraySlice      = range.layers_range.x,
        .NumArraySlices       = 1,
        .FirstPlane           = planes.x,
        .NumPlanes            = planes.y,
    };
}

D3D_PRIMITIVE_TOPOLOGY platform::d3d12_rhi_parse_topology(const rhi::topology topology)
{
    ZoneScoped;
    switch (topology)
    {
    case rhi::topology::list_lines :
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case rhi::topology::list_triangles :
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case rhi::topology::list_lines_adjacent :
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
    case rhi::topology::list_triangles_adjacent :
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
    case rhi::topology::strip_lines :
        return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case rhi::topology::strip_triangles :
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case rhi::topology::strip_lines_adjacent :
        return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
    case rhi::topology::strip_triangles_adjacent :
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
    default :
    case rhi::topology::list_points :
        return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    }
}

D3D12_COMPARISON_FUNC platform::d3d12_rhi_parse_comparison_func(const rhi::compare_op op)
{
    ZoneScoped;
    switch (op)
    {
    case rhi::compare_op::never :
        return D3D12_COMPARISON_FUNC_NEVER;
    case rhi::compare_op::always :
        return D3D12_COMPARISON_FUNC_ALWAYS;
    case rhi::compare_op::less :
        return D3D12_COMPARISON_FUNC_LESS;
    case rhi::compare_op::greater :
        return D3D12_COMPARISON_FUNC_GREATER;
    case rhi::compare_op::equal :
        return D3D12_COMPARISON_FUNC_EQUAL;
    case rhi::compare_op::not_equal :
        return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case rhi::compare_op::equal_or_less :
        return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case rhi::compare_op::equal_or_greater :
        return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    default :
    case rhi::compare_op::none :
        return D3D12_COMPARISON_FUNC_NONE;
    }
}

D3D12_BARRIER_SYNC platform::d3d12_rhi_parse_stage_flags(const rhi::barrier_stage_bits stages)
{
    ZoneScoped;
    D3D12_BARRIER_SYNC result = D3D12_BARRIER_SYNC_NONE;
    for (u32 i = 0; i < reflection::get_enum_values_count<rhi::barrier_stage>(); i++)
    {
        const auto flag = reflection::get_enum_value_at<rhi::barrier_stage>(i);
        if (!(flag & stages))
        {
            continue;
        }

        switch (flag)
        {
        case rhi::barrier_stage::indirect :
            result |= D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
            break;
        case rhi::barrier_stage::vertex_input :
            result |= D3D12_BARRIER_SYNC_INDEX_INPUT | D3D12_BARRIER_SYNC_VERTEX_SHADING;
            break;
        case rhi::barrier_stage::task_shader :
        case rhi::barrier_stage::mesh_shader :
        case rhi::barrier_stage::vertex_shader :
            result |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
            break;
        case rhi::barrier_stage::fragment_shader :
            result |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
            break;
        case rhi::barrier_stage::compute_shader :
            result |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
            break;
        case rhi::barrier_stage::late_depth_stencil :
        case rhi::barrier_stage::early_depth_stencil :
            result |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
            break;
        case rhi::barrier_stage::color_attachment :
            result |= D3D12_BARRIER_SYNC_RENDER_TARGET;
            break;
        case rhi::barrier_stage::copy :
            result |= D3D12_BARRIER_SYNC_COPY;
            break;
        case rhi::barrier_stage::all_graphics :
            result |= D3D12_BARRIER_SYNC_DRAW | D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
            break;
        case rhi::barrier_stage::all_commands :
            result |= D3D12_BARRIER_SYNC_ALL;
            break;
        default :
        case rhi::barrier_stage::none :
            break;
        }
    }
    return result;
}

D3D12_BARRIER_ACCESS platform::d3d12_rhi_parse_access_flags(const rhi::barrier_access_bits access)
{
    ZoneScoped;
    D3D12_BARRIER_ACCESS result = D3D12_BARRIER_ACCESS_COMMON;
    for (u32 i = 0; i < reflection::get_enum_values_count<rhi::barrier_access>(); i++)
    {
        const auto flag = reflection::get_enum_value_at<rhi::barrier_access>(i);
        if (!(flag & access))
        {
            continue;
        }

        switch (flag)
        {
        case rhi::barrier_access::indirect_read :
            result |= D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
            break;
        case rhi::barrier_access::index_read :
            result |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
            break;
        case rhi::barrier_access::vertex_read :
            result |= D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
            break;
        case rhi::barrier_access::uniform_read :
            result |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
            break;
        case rhi::barrier_access::sampled_read :
            result |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
            break;
        case rhi::barrier_access::storage_read :
        case rhi::barrier_access::storage_write :
            result |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
            break;
        case rhi::barrier_access::color_attachment_read :
        case rhi::barrier_access::color_attachment_write :
            result |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
            break;
        case rhi::barrier_access::depth_stencil_read :
            result |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
            break;
        case rhi::barrier_access::depth_stencil_write :
            result |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
            break;
        case rhi::barrier_access::copy_read :
            result |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
            break;
        case rhi::barrier_access::copy_write :
            result |= D3D12_BARRIER_ACCESS_COPY_DEST;
            break;
        default :
        case rhi::barrier_access::none :
            break;
        }
    }

    return result == D3D12_BARRIER_ACCESS_COMMON ? D3D12_BARRIER_ACCESS_NO_ACCESS : result;
}

D3D12_FILTER_TYPE platform::d3d12_rhi_parse_filter(const rhi::sampler_filter filter)
{
    ZoneScoped;
    switch (filter)
    {
    case rhi::sampler_filter::nearest :
        return D3D12_FILTER_TYPE_POINT;
    default :
    case rhi::sampler_filter::linear :
        return D3D12_FILTER_TYPE_LINEAR;
    }
}

D3D12_FILTER_TYPE platform::d3d12_rhi_parse_mipmap(const rhi::sampler_mipmap_mode mode)
{
    ZoneScoped;
    switch (mode)
    {
    default :;
    case rhi::sampler_mipmap_mode::linear :
        return D3D12_FILTER_TYPE_LINEAR;
    case rhi::sampler_mipmap_mode::nearest :
        return D3D12_FILTER_TYPE_POINT;
    }
}

D3D12_FILTER_REDUCTION_TYPE platform::d3d12_rhi_parse_reduction(const rhi::sampler_reduction reduction)
{
    ZoneScoped;
    switch (reduction)
    {
    case rhi::sampler_reduction::min :
        return D3D12_FILTER_REDUCTION_TYPE_MINIMUM;
    case rhi::sampler_reduction::max :
        return D3D12_FILTER_REDUCTION_TYPE_MAXIMUM;
    default :
    case rhi::sampler_reduction::none :
        return D3D12_FILTER_REDUCTION_TYPE_STANDARD;
    }
}

D3D12_TEXTURE_ADDRESS_MODE platform::d3d12_rhi_parse_address_mode(const rhi::sampler_address_mode address_mode)
{
    ZoneScoped;
    switch (address_mode)
    {
    case rhi::sampler_address_mode::clamp_to_edge :
        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case rhi::sampler_address_mode::clamp_to_border :
        return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    case rhi::sampler_address_mode::mirrored_repeat :
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    case rhi::sampler_address_mode::mirrored_clamp_to_edge :
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
    default :
    case rhi::sampler_address_mode::repeat :
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}

D3D12_FILTER platform::d3d12_rhi_parse_filter_type(const rhi::sampler_filter filter,
                                                   const rhi::sampler_reduction reduction,
                                                   const rhi::sampler_mipmap_mode mipmap_mode)
{
    ZoneScoped;
    const D3D12_FILTER_TYPE min_mag       = d3d12_rhi_parse_filter(filter);
    const D3D12_FILTER_TYPE mip           = d3d12_rhi_parse_mipmap(mipmap_mode);
    const D3D12_FILTER_REDUCTION_TYPE red = d3d12_rhi_parse_reduction(reduction);

    return D3D12_ENCODE_BASIC_FILTER(min_mag, min_mag, mip, red);
}
