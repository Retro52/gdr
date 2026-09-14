#pragma once

#include <volk.h>

#include <types.hpp>

#include <reflection/enum.hpp>
#include <render/resources.hpp>

namespace render::rhi
{
    // clang-format off
    REGISTER_FLAGS(feature_flag,
        eValidation        = 1 << 0,
        eMeshShading       = 1 << 1,
        eDynamicRender     = 1 << 2,
        eSynchronization2  = 1 << 3,
        eDrawIndirect      = 1 << 4,
        e8BitIntegers      = 1 << 5,
        ePipelineStats     = 1 << 6,
        eSamplerMinMax     = 1 << 7,
        eScalarBlockLayout = 1 << 8,
        ePortabilitySubset = 1 << 9,
        eBindlessTextures  = 1 << 10,
        e16BitTypes        = 1 << 11,
        COUNT
    );

    REGISTER_ENUM(queue_kind,
        ePresent  = 0,
        eTransfer = 1,
        eGfx      = 2,
        eCompute  = 3,
        COUNT     = 4
    );

    REGISTER_ENUM(image_kind,
        e2D,
        e3D,
        eCube,
        COUNT
    );

    REGISTER_ENUM(image_layout,
        eCommon,
        ePresent,
        COUNT
    );

    REGISTER_FLAGS(image_usage,
        eSampled         = 1 << 0,
        eStorage         = 1 << 1,
        eAttachmentColor = 1 << 2,
        eAttachmentDS    = 1 << 3,
        eTransferSrc     = 1 << 4,
        eTransferDst     = 1 << 5,
        COUNT
    );
    // clang-format on

    constexpr u32 kQueueTypesCount = static_cast<u32>(queue_kind::COUNT);

    struct create_image_info
    {
        image_kind kind   = image_kind::e2D;
        image_usage usage = image_usage::eSampled;
        uvec3 dimensions  = uvec3(1, 1, 1);
        u32 mips_count    = 1;
        u32 layer_count   = 1;
        // I hate these custom enums that try to cover a gajillion different formats from the API they abstract, so fuck
        // it, VkFormat is my new universal enum
        VkFormat format = VK_FORMAT_UNDEFINED;
    };

    struct create_swapchain_info
    {
        ivec2 size;
        u32 frames_in_flight           = 2;
        VkFormat format                = VK_FORMAT_R8G8B8A8_UNORM;
        bool vsync                     = false;
        const swapchain* old_swapchain = nullptr;
    };

    struct rendering_features_table
    {
        [[nodiscard]] constexpr bool required(const feature_flag flag) const noexcept
        {
            return (flag & required_features) > 0;
        }

        [[nodiscard]] constexpr bool requested(const feature_flag flag) const noexcept
        {
            return (flag & requested_features) > 0 || required(flag);
        }

        [[nodiscard]] constexpr bool supported(const feature_flag flag) const noexcept
        {
            return (flag & supported_features) > 0;
        }

        [[nodiscard]] constexpr bool wanted(const feature_flag flag) const noexcept
        {
            return (flag & supported_features) > 0 && requested(flag);
        }

        constexpr rendering_features_table& require(const feature_flag flag) noexcept
        {
            required_features = required_features | flag;
            return *this;
        }

        constexpr rendering_features_table& request(const feature_flag flag) noexcept
        {
            requested_features = requested_features | flag;
            return *this;
        }

        constexpr void set_supported(const feature_flag flag, const bool supported = true) noexcept
        {
            supported_features = supported ? supported_features | flag : supported_features & ~flag;
        }

        [[nodiscard]] constexpr bool all_required_supported() const noexcept
        {
            bool result = true;
            for (u32 i = 0; i < cpp::cx_get_enum_bit_count(feature_flag::COUNT); ++i)
            {
                const auto feature = static_cast<feature_flag>(1 << i);
                result &= this->required(feature) ? this->supported(feature) : result;
            }

            return result;
        }

        u32 required_features  = 0;
        u32 requested_features = 0;
        u32 supported_features = 0;
    };

    struct instance_desc
    {
        const char* app_name = "";
        u32 app_version      = 0;
        u32 device_id_hint   = static_cast<u32>(-1);
        rendering_features_table device_features;
    };
}
