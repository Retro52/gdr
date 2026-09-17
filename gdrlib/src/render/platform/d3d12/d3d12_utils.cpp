#pragma once

#include <ddspp.h>
#include <render/platform/d3d12/d3d12_utils.hpp>

u32 render::d3d12_format_from_vk(const VkFormat vk_format)
{
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
