#include <ddspp.h>
#include <render/platform/vk/vk_utils.hpp>

#define COLOR_SPACE_PAIR(linear_format, srgb_format)                     \
    case linear_format :                                                 \
        return space == color_space::srgb ? srgb_format : linear_format; \
    case srgb_format :                                                   \
        return space == color_space::linear ? linear_format : srgb_format

VkFormat render::vk_format_from_dxgi(const u32 dx_format)
{
    using ddspp::DXGIFormat;

    switch (dx_format)
    {
    case ddspp::R32G32B32A32_FLOAT :
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case ddspp::R32G32B32A32_UINT :
        return VK_FORMAT_R32G32B32A32_UINT;
    case ddspp::R32G32B32A32_SINT :
        return VK_FORMAT_R32G32B32A32_SINT;
    case ddspp::R32G32B32_FLOAT :
        return VK_FORMAT_R32G32B32_SFLOAT;
    case ddspp::R32G32B32_UINT :
        return VK_FORMAT_R32G32B32_UINT;
    case ddspp::R32G32B32_SINT :
        return VK_FORMAT_R32G32B32_SINT;
    case ddspp::R16G16B16A16_FLOAT :
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case ddspp::R16G16B16A16_UNORM :
        return VK_FORMAT_R16G16B16A16_UNORM;
    case ddspp::R16G16B16A16_UINT :
        return VK_FORMAT_R16G16B16A16_UINT;
    case ddspp::R16G16B16A16_SNORM :
        return VK_FORMAT_R16G16B16A16_SNORM;
    case ddspp::R16G16B16A16_SINT :
        return VK_FORMAT_R16G16B16A16_SINT;
    case ddspp::R32G32_FLOAT :
        return VK_FORMAT_R32G32_SFLOAT;
    case ddspp::R32G32_UINT :
        return VK_FORMAT_R32G32_UINT;
    case ddspp::R32G32_SINT :
        return VK_FORMAT_R32G32_SINT;
    case ddspp::D32_FLOAT_S8X24_UINT :
        return VK_FORMAT_D32_SFLOAT_S8_UINT;
    case ddspp::R10G10B10A2_UNORM :
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case ddspp::R10G10B10A2_UINT :
        return VK_FORMAT_A2B10G10R10_UINT_PACK32;
    case ddspp::R11G11B10_FLOAT :
        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case ddspp::R8G8B8A8_UNORM :
        return VK_FORMAT_R8G8B8A8_UNORM;
    case ddspp::R8G8B8A8_UNORM_SRGB :
        return VK_FORMAT_R8G8B8A8_SRGB;
    case ddspp::R8G8B8A8_UINT :
        return VK_FORMAT_R8G8B8A8_UINT;
    case ddspp::R8G8B8A8_SNORM :
        return VK_FORMAT_R8G8B8A8_SNORM;
    case ddspp::R8G8B8A8_SINT :
        return VK_FORMAT_R8G8B8A8_SINT;
    case ddspp::R16G16_FLOAT :
        return VK_FORMAT_R16G16_SFLOAT;
    case ddspp::R16G16_UNORM :
        return VK_FORMAT_R16G16_UNORM;
    case ddspp::R16G16_UINT :
        return VK_FORMAT_R16G16_UINT;
    case ddspp::R16G16_SNORM :
        return VK_FORMAT_R16G16_SNORM;
    case ddspp::R16G16_SINT :
        return VK_FORMAT_R16G16_SINT;
    case ddspp::D32_FLOAT :
        return VK_FORMAT_D32_SFLOAT;
    case ddspp::R32_FLOAT :
        return VK_FORMAT_R32_SFLOAT;
    case ddspp::R32_UINT :
        return VK_FORMAT_R32_UINT;
    case ddspp::R32_SINT :
        return VK_FORMAT_R32_SINT;
    case ddspp::D24_UNORM_S8_UINT :
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case ddspp::R8G8_UNORM :
        return VK_FORMAT_R8G8_UNORM;
    case ddspp::R8G8_UINT :
        return VK_FORMAT_R8G8_UINT;
    case ddspp::R8G8_SNORM :
        return VK_FORMAT_R8G8_SNORM;
    case ddspp::R8G8_SINT :
        return VK_FORMAT_R8G8_SINT;
    case ddspp::R16_FLOAT :
        return VK_FORMAT_R16_SFLOAT;
    case ddspp::D16_UNORM :
        return VK_FORMAT_D16_UNORM;
    case ddspp::R16_UNORM :
        return VK_FORMAT_R16_UNORM;
    case ddspp::R16_UINT :
        return VK_FORMAT_R16_UINT;
    case ddspp::R16_SNORM :
        return VK_FORMAT_R16_SNORM;
    case ddspp::R16_SINT :
        return VK_FORMAT_R16_SINT;
    case ddspp::R8_UNORM :
        return VK_FORMAT_R8_UNORM;
    case ddspp::R8_UINT :
        return VK_FORMAT_R8_UINT;
    case ddspp::R8_SNORM :
        return VK_FORMAT_R8_SNORM;
    case ddspp::R8_SINT :
        return VK_FORMAT_R8_SINT;
    case ddspp::A8_UNORM :
        return VK_FORMAT_A8_UNORM;
    case ddspp::R9G9B9E5_SHAREDEXP :
        return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
    case ddspp::R8G8_B8G8_UNORM :
        return VK_FORMAT_G8B8G8R8_422_UNORM;
    case ddspp::G8R8_G8B8_UNORM :
        return VK_FORMAT_B8G8R8G8_422_UNORM;
    case ddspp::BC1_UNORM :
        return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case ddspp::BC1_UNORM_SRGB :
        return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case ddspp::BC2_UNORM :
        return VK_FORMAT_BC2_UNORM_BLOCK;
    case ddspp::BC2_UNORM_SRGB :
        return VK_FORMAT_BC2_SRGB_BLOCK;
    case ddspp::BC3_UNORM :
        return VK_FORMAT_BC3_UNORM_BLOCK;
    case ddspp::BC3_UNORM_SRGB :
        return VK_FORMAT_BC3_SRGB_BLOCK;
    case ddspp::BC4_UNORM :
        return VK_FORMAT_BC4_UNORM_BLOCK;
    case ddspp::BC4_SNORM :
        return VK_FORMAT_BC4_SNORM_BLOCK;
    case ddspp::BC5_UNORM :
        return VK_FORMAT_BC5_UNORM_BLOCK;
    case ddspp::BC5_SNORM :
        return VK_FORMAT_BC5_SNORM_BLOCK;
    case ddspp::B5G6R5_UNORM :
        return VK_FORMAT_B5G6R5_UNORM_PACK16;
    case ddspp::B5G5R5A1_UNORM :
        return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
    case ddspp::B8G8R8A8_UNORM :
    case ddspp::B8G8R8X8_UNORM :
        return VK_FORMAT_B8G8R8A8_UNORM;
    case ddspp::B8G8R8A8_UNORM_SRGB :
    case ddspp::B8G8R8X8_UNORM_SRGB :
        return VK_FORMAT_B8G8R8A8_SRGB;
    case ddspp::BC6H_UF16 :
        return VK_FORMAT_BC6H_UFLOAT_BLOCK;
    case ddspp::BC6H_SF16 :
        return VK_FORMAT_BC6H_SFLOAT_BLOCK;
    case ddspp::BC7_UNORM :
        return VK_FORMAT_BC7_UNORM_BLOCK;
    case ddspp::BC7_UNORM_SRGB :
        return VK_FORMAT_BC7_SRGB_BLOCK;
    case ddspp::NV12 :
        return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
    case ddspp::P010 :
        return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
    case ddspp::P016 :
        return VK_FORMAT_G16_B16R16_2PLANE_420_UNORM;
    case ddspp::YUY2 :
        return VK_FORMAT_G8B8G8R8_422_UNORM;
    case ddspp::Y210 :
        return VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16;
    case ddspp::Y216 :
        return VK_FORMAT_G16B16G16R16_422_UNORM;
    case ddspp::B4G4R4A4_UNORM :
        return VK_FORMAT_B4G4R4A4_UNORM_PACK16;

    // Xbox-specific
    case ddspp::D16_UNORM_S8_UINT :
        return VK_FORMAT_D16_UNORM_S8_UINT;

    case ddspp::P208 :
        return VK_FORMAT_G8_B8R8_2PLANE_422_UNORM;
    case ddspp::V208 :
        return VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM;
    case ddspp::V408 :
        return VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM;
    case ddspp::ASTC_4X4_UNORM :
        return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
    case ddspp::ASTC_4X4_UNORM_SRGB :
        return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
    case ddspp::ASTC_5X4_UNORM :
        return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
    case ddspp::ASTC_5X4_UNORM_SRGB :
        return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
    case ddspp::ASTC_5X5_UNORM :
        return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
    case ddspp::ASTC_5X5_UNORM_SRGB :
        return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;

    case ddspp::ASTC_6X5_UNORM :
        return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
    case ddspp::ASTC_6X5_UNORM_SRGB :
        return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
    case ddspp::ASTC_6X6_UNORM :
        return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
    case ddspp::ASTC_6X6_UNORM_SRGB :
        return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
    case ddspp::ASTC_8X5_UNORM :
        return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
    case ddspp::ASTC_8X5_UNORM_SRGB :
        return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
    case ddspp::ASTC_8X6_UNORM :
        return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
    case ddspp::ASTC_8X6_UNORM_SRGB :
        return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
    case ddspp::ASTC_8X8_UNORM :
        return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
    case ddspp::ASTC_8X8_UNORM_SRGB :
        return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
    case ddspp::ASTC_10X5_UNORM :
        return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
    case ddspp::ASTC_10X5_UNORM_SRGB :
        return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
    case ddspp::ASTC_10X6_UNORM :
        return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
    case ddspp::ASTC_10X6_UNORM_SRGB :
        return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
    case ddspp::ASTC_10X8_UNORM :
        return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
    case ddspp::ASTC_10X8_UNORM_SRGB :
        return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
    case ddspp::ASTC_10X10_UNORM :
        return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
    case ddspp::ASTC_10X10_UNORM_SRGB :
        return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
    case ddspp::ASTC_12X10_UNORM :
        return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
    case ddspp::ASTC_12X10_UNORM_SRGB :
        return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
    case ddspp::ASTC_12X12_UNORM :
        return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
    case ddspp::ASTC_12X12_UNORM_SRGB :
        return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
    case ddspp::UNKNOWN :
    case ddspp::FORCE_UINT :
    default :
        return VK_FORMAT_UNDEFINED;
    }
}

VkFormat render::vk_format_force_color_space(const VkFormat vk_format, const color_space space)
{
    switch (vk_format)
    {
        COLOR_SPACE_PAIR(VK_FORMAT_R8_UNORM, VK_FORMAT_R8_SRGB);
        COLOR_SPACE_PAIR(VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8_SRGB);
        COLOR_SPACE_PAIR(VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8_SRGB);
        COLOR_SPACE_PAIR(VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_B8G8R8_SRGB);
        COLOR_SPACE_PAIR(VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB);
        COLOR_SPACE_PAIR(VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB);
        COLOR_SPACE_PAIR(VK_FORMAT_A8B8G8R8_UNORM_PACK32, VK_FORMAT_A8B8G8R8_SRGB_PACK32);

        COLOR_SPACE_PAIR(VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_BC1_RGB_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_FORMAT_BC1_RGBA_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_BC2_UNORM_BLOCK, VK_FORMAT_BC2_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_BC3_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_BC7_UNORM_BLOCK, VK_FORMAT_BC7_SRGB_BLOCK);

        COLOR_SPACE_PAIR(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);

        COLOR_SPACE_PAIR(VK_FORMAT_ASTC_4x4_UNORM_BLOCK, VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_ASTC_5x4_UNORM_BLOCK, VK_FORMAT_ASTC_5x4_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_ASTC_5x5_UNORM_BLOCK, VK_FORMAT_ASTC_5x5_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_ASTC_6x5_UNORM_BLOCK, VK_FORMAT_ASTC_6x5_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_ASTC_6x6_UNORM_BLOCK, VK_FORMAT_ASTC_6x6_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_ASTC_8x5_UNORM_BLOCK, VK_FORMAT_ASTC_8x5_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_ASTC_8x6_UNORM_BLOCK, VK_FORMAT_ASTC_8x6_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_ASTC_8x8_UNORM_BLOCK, VK_FORMAT_ASTC_8x8_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_ASTC_10x5_UNORM_BLOCK, VK_FORMAT_ASTC_10x5_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_ASTC_10x6_UNORM_BLOCK, VK_FORMAT_ASTC_10x6_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_ASTC_10x8_UNORM_BLOCK, VK_FORMAT_ASTC_10x8_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_ASTC_10x10_UNORM_BLOCK, VK_FORMAT_ASTC_10x10_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_ASTC_12x10_UNORM_BLOCK, VK_FORMAT_ASTC_12x10_SRGB_BLOCK);
        COLOR_SPACE_PAIR(VK_FORMAT_ASTC_12x12_UNORM_BLOCK, VK_FORMAT_ASTC_12x12_SRGB_BLOCK);

    default :
        return vk_format;
    }
}

VkImageViewType render::vk_rhi_parse_image_view_type(const render::rhi::image_view_kind kind)
{
    switch (kind)
    {
    case render::rhi::image_view_kind::flat_1d :
        return VK_IMAGE_VIEW_TYPE_1D;
    case render::rhi::image_view_kind::array_1d :
        return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    case render::rhi::image_view_kind::flat_2d :
        return VK_IMAGE_VIEW_TYPE_2D;
    case render::rhi::image_view_kind::array_2d :
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case render::rhi::image_view_kind::flat_cube :
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case render::rhi::image_view_kind::array_cube :
        return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    case render::rhi::image_view_kind::flat_3d :
        return VK_IMAGE_VIEW_TYPE_3D;
    default :
        break;
    }

    return VK_IMAGE_VIEW_TYPE_2D;
}

VkImageAspectFlags render::vk_rhi_parse_aspect_flags(const render::rhi::image_aspects aspects)
{
    VkImageAspectFlags result = 0;
    assert2(!(aspects & render::rhi::image_aspect::format));

    result |= aspects & render::rhi::image_aspect::color ? VK_IMAGE_ASPECT_COLOR_BIT : result;
    result |= aspects & render::rhi::image_aspect::depth ? VK_IMAGE_ASPECT_DEPTH_BIT : result;
    result |= aspects & render::rhi::image_aspect::stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : result;

    return result;
}

VkImageAspectFlags render::vk_rhi_parse_format_aspect_flags(const VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_S8_UINT :
        return VK_IMAGE_ASPECT_STENCIL_BIT;
    case VK_FORMAT_D16_UNORM :
    case VK_FORMAT_D32_SFLOAT :
    case VK_FORMAT_X8_D24_UNORM_PACK32 :
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT :
    case VK_FORMAT_D24_UNORM_S8_UINT :
    case VK_FORMAT_D32_SFLOAT_S8_UINT :
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default :
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

render::rhi::shader_stage render::vk_rhi_translate_shader_stage(const VkShaderStageFlagBits stage)
{
    switch (stage)
    {
    case VK_SHADER_STAGE_TASK_BIT_EXT :
        return render::rhi::shader_stage::task;
    case VK_SHADER_STAGE_MESH_BIT_EXT :
        return render::rhi::shader_stage::mesh;
    case VK_SHADER_STAGE_COMPUTE_BIT :
        return render::rhi::shader_stage::compute;
    case VK_SHADER_STAGE_FRAGMENT_BIT :
        return render::rhi::shader_stage::fragment;
    default :
    case VK_SHADER_STAGE_VERTEX_BIT :
        return render::rhi::shader_stage::vertex;
    }
}

VkImageUsageFlags render::vk_rhi_parse_image_usage_flags(const render::rhi::image_usages usage)
{
    VkImageUsageFlags result = 0;
    for (u32 i = 0; i < reflection::get_enum_values_count<render::rhi::image_usage>(); i++)
    {
        const auto flag = reflection::get_enum_value_at<render::rhi::image_usage>(i);
        if (!(flag & usage))
        {
            continue;
        }

        switch (flag)
        {
        case render::rhi::image_usage::sampled :
            result |= VK_IMAGE_USAGE_SAMPLED_BIT;
            break;
        case render::rhi::image_usage::storage :
            result |= VK_IMAGE_USAGE_STORAGE_BIT;
            break;
        case render::rhi::image_usage::attachment_color :
            result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            break;
        case render::rhi::image_usage::attachment_ds :
            result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            break;
        case render::rhi::image_usage::transfer_src :
            result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            break;
        case render::rhi::image_usage::transfer_dst :
            result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            break;
        default :
            break;
        }
    }

    return result;
}

VkBufferUsageFlags render::vk_rhi_parse_buffer_usage_flags(const render::rhi::buffer_usages usage)
{
    VkBufferUsageFlags result = 0;
    for (u32 i = 0; i < reflection::get_enum_values_count<render::rhi::buffer_usage>(); i++)
    {
        const auto flag = reflection::get_enum_value_at<render::rhi::buffer_usage>(i);
        if (!(flag & usage))
        {
            continue;
        }

        switch (flag)
        {
        case render::rhi::buffer_usage::copy_src :
            result |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            break;
        case render::rhi::buffer_usage::copy_dst :
            result |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            break;
        case render::rhi::buffer_usage::shader_rw :
            result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            break;
        case render::rhi::buffer_usage::indirect :
            result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            break;
        case render::rhi::buffer_usage::index :
            result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            break;
        default :
            break;
        }
    }

    return result;
}

VkFilter render::vk_rhi_parse_sampler_filter(const render::rhi::sampler_filter sampler_filter)
{
    switch (sampler_filter)
    {
    case render::rhi::sampler_filter::nearest :
        return VK_FILTER_NEAREST;
    default :
    case render::rhi::sampler_filter::linear :
        return VK_FILTER_LINEAR;
    }
}

VkBorderColor render::vk_rhi_parse_sampler_border_color(const render::rhi::sampler_border_color sampler_border_color)
{
    switch (sampler_border_color)
    {
    case render::rhi::sampler_border_color::white :
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    case render::rhi::sampler_border_color::black :
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    default :
    case render::rhi::sampler_border_color::black_transparent :
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    }
}

VkSamplerReductionMode render::vk_rhi_parse_sampler_reduction(const render::rhi::sampler_reduction sampler_reduction)
{
    switch (sampler_reduction)
    {
    case render::rhi::sampler_reduction::min :
        return VK_SAMPLER_REDUCTION_MODE_MIN;
    case render::rhi::sampler_reduction::max :
        return VK_SAMPLER_REDUCTION_MODE_MAX;
    default :
    case render::rhi::sampler_reduction::none :
        return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
    }
}

VkSamplerMipmapMode render::vk_rhi_parse_sampler_mipmap_mode(const render::rhi::sampler_mipmap_mode sampler_mipmap_mode)
{
    switch (sampler_mipmap_mode)
    {
    case render::rhi::sampler_mipmap_mode::nearest :
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    default :
    case render::rhi::sampler_mipmap_mode::linear :
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

VkSamplerAddressMode render::vk_rhi_parse_sampler_address_mode(
    const render::rhi::sampler_address_mode sampler_address_mode)
{
    switch (sampler_address_mode)
    {
    case render::rhi::sampler_address_mode::clamp_to_edge :
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case render::rhi::sampler_address_mode::clamp_to_border :
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case render::rhi::sampler_address_mode::mirrored_repeat :
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case render::rhi::sampler_address_mode::mirrored_clamp_to_edge :
        return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    default :
    case render::rhi::sampler_address_mode::repeat :
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

VkCompareOp render::vk_rhi_parse_compare_op(const render::rhi::compare_op compare_op)
{
    switch (compare_op)
    {
    case render::rhi::compare_op::always :
        return VK_COMPARE_OP_ALWAYS;
    case render::rhi::compare_op::less :
        return VK_COMPARE_OP_LESS;
    case render::rhi::compare_op::greater :
        return VK_COMPARE_OP_GREATER;
    case render::rhi::compare_op::equal :
        return VK_COMPARE_OP_EQUAL;
    case render::rhi::compare_op::not_equal :
        return VK_COMPARE_OP_NOT_EQUAL;
    case render::rhi::compare_op::equal_or_less :
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case render::rhi::compare_op::equal_or_greater :
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    default :
    case render::rhi::compare_op::none :
    case render::rhi::compare_op::never :
        return VK_COMPARE_OP_NEVER;
    }
}

VkClearValue render::vk_rhi_parse_color_clear_value(render::rhi::clear_value cv)
{
    VkClearValue value;
    cpp::cx_memcpy(value.color.float32, &cv.color.f4.x, COUNT_OF(value.color.float32) * sizeof(cv.color.f4[0]));
    return value;
}

VkClearValue render::vk_rhi_parse_depth_clear_value(const render::rhi::clear_value cv)
{
    return {
        .depthStencil = {.depth = cv.ds.depth, .stencil = cv.ds.stencil}
    };
}

VkPrimitiveTopology render::vk_rhi_parse_topology(const render::rhi::topology topology)
{
    switch (topology)
    {
    case render::rhi::topology::list_points :
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case render::rhi::topology::list_patches :
        return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

    case render::rhi::topology::list_lines :
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case render::rhi::topology::list_lines_adjacent :
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
    case render::rhi::topology::list_triangles_adjacent :
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;

    case render::rhi::topology::strip_lines :
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case render::rhi::topology::strip_triangles :
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case render::rhi::topology::strip_lines_adjacent :
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
    case render::rhi::topology::strip_triangles_adjacent :
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;

    default :
    case render::rhi::topology::list_triangles :
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

VkBlendOp render::vk_rhi_parse_blend_op(const render::rhi::blend_op blend_op)
{
    switch (blend_op)
    {
    case render::rhi::blend_op::max :
        return VK_BLEND_OP_MAX;
    case render::rhi::blend_op::add :
        return VK_BLEND_OP_ADD;
    case render::rhi::blend_op::subtract :
        return VK_BLEND_OP_SUBTRACT;
    case render::rhi::blend_op::reverse_subtract :
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    default :
    case render::rhi::blend_op::min :
        return VK_BLEND_OP_MIN;
    }
}

VkBlendFactor render::vk_rhi_parse_blend_factor(const render::rhi::blend_factor blend_factor)
{
    switch (blend_factor)
    {
    case render::rhi::blend_factor::one :
        return VK_BLEND_FACTOR_ONE;
    case render::rhi::blend_factor::src_color :
        return VK_BLEND_FACTOR_SRC_COLOR;
    case render::rhi::blend_factor::src_alpha :
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case render::rhi::blend_factor::one_minus_src_color :
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case render::rhi::blend_factor::one_minus_src_alpha :
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case render::rhi::blend_factor::dst_color :
        return VK_BLEND_FACTOR_DST_COLOR;
    case render::rhi::blend_factor::dst_alpha :
        return VK_BLEND_FACTOR_DST_ALPHA;
    case render::rhi::blend_factor::one_minus_dst_color :
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case render::rhi::blend_factor::one_minus_dst_alpha :
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    default :
    case render::rhi::blend_factor::zero :
        return VK_BLEND_FACTOR_ZERO;
    }
}
