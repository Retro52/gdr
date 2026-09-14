#include <ddspp.h>
#include <render/platform/vk/vk_utils.hpp>

#define COLOR_SPACE_PAIR(linear_format, srgb_format)                     \
    case linear_format :                                                 \
        return space == color_space::srgb ? srgb_format : linear_format; \
    case srgb_format :                                                   \
        return space == color_space::linear ? linear_format : srgb_format

VkFormat render::vk_format_from_dxgi(const u32 gx_format)
{
    using ddspp::DXGIFormat;

    switch (gx_format)
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
