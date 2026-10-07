#pragma once

#include <volk.h>

#include <reflection/enum.hpp>

namespace render::rhi
{
    // clang-format off
    REGISTER_FLAGS(pso_flag,
        eDepthBias   = 1 << 0,
        eDepthClamp  = 1 << 1,
        eDepthWrite  = 1 << 2,
        eDepthTest   = 1 << 3,
        eBlendEnable = 1 << 4
    );
    // clang-format on

#define DECLARE_SIMPLE_PSO_OPTION(TYPE, NAME, DEFAULT) \
    TYPE NAME = DEFAULT;                               \
    pso_options& set_##NAME(const TYPE& value)         \
    {                                                  \
        this->NAME = value;                            \
        return *this;                                  \
    }

    struct pso_options
    {
        u32 color_attachments_count = 0;
        VkFormat color_formats[8]   = {};
        pso_flags flags             = pso_flag::eDepthTest | pso_flag::eDepthWrite;

        DECLARE_SIMPLE_PSO_OPTION(VkFormat, depth_format, VK_FORMAT_UNDEFINED)
        DECLARE_SIMPLE_PSO_OPTION(VkCompareOp, depth_compare_op, VK_COMPARE_OP_GREATER)
        DECLARE_SIMPLE_PSO_OPTION(VkPrimitiveTopology, topology, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)

        DECLARE_SIMPLE_PSO_OPTION(VkBlendOp, color_blend_op, VK_BLEND_OP_ADD)
        DECLARE_SIMPLE_PSO_OPTION(VkBlendFactor, src_color_blend_factor, VK_BLEND_FACTOR_ONE)
        DECLARE_SIMPLE_PSO_OPTION(VkBlendFactor, dst_color_blend_factor, VK_BLEND_FACTOR_ZERO)

        DECLARE_SIMPLE_PSO_OPTION(VkBlendOp, alpha_blend_op, VK_BLEND_OP_ADD)
        DECLARE_SIMPLE_PSO_OPTION(VkBlendFactor, src_alpha_blend_factor, VK_BLEND_FACTOR_ONE)
        DECLARE_SIMPLE_PSO_OPTION(VkBlendFactor, dst_alpha_blend_factor, VK_BLEND_FACTOR_ZERO)

        pso_options& set_flag(const pso_flag flag, const bool enable = true)
        {
            this->flags = enable ? this->flags | flag : this->flags & ~flag;
            return *this;
        }

        pso_options& add_color_attachment(const VkFormat color_format)
        {
            this->color_formats[this->color_attachments_count++] = color_format;
            return *this;
        }
    };

#undef DECLARE_SIMPLE_PSO_OPTION
}
