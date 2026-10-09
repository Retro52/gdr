#pragma once

#include <volk.h>

#include <reflection/enum.hpp>
#include <render/types.hpp>

namespace rhi
{
#define DECLARE_SIMPLE_PSO_OPTION(TYPE, NAME, DEFAULT) \
    TYPE NAME = DEFAULT;                               \
    pso_options& set_##NAME(const TYPE& value)         \
    {                                                  \
        this->NAME = value;                            \
        return *this;                                  \
    }

#define DECLARE_BIT_FLAG_PSO_OPTION(NAME, DEFAULT) \
    u32 NAME : 1 = DEFAULT;                        \
    pso_options& set_##NAME(const bool value)      \
    {                                              \
        this->NAME = value;                        \
        return *this;                              \
    }

    struct pso_options
    {
        u32 color_attachments_count        = 0;
        rhi::image_format color_formats[8] = {};

        DECLARE_BIT_FLAG_PSO_OPTION(depth_test_enable, true);
        DECLARE_BIT_FLAG_PSO_OPTION(depth_write_enable, true);
        DECLARE_BIT_FLAG_PSO_OPTION(depth_bias_enable, false);
        DECLARE_BIT_FLAG_PSO_OPTION(depth_clamp_enable, false);
        DECLARE_BIT_FLAG_PSO_OPTION(blend_enable, false);
        u32 padding : 27 = 0;

        DECLARE_SIMPLE_PSO_OPTION(rhi::image_format, depth_format, image_format::none)
        DECLARE_SIMPLE_PSO_OPTION(rhi::compare_op, depth_compare_op, compare_op::greater)
        DECLARE_SIMPLE_PSO_OPTION(rhi::topology, topology, topology::list_triangles)

        DECLARE_SIMPLE_PSO_OPTION(rhi::blend_op, color_blend_op, rhi::blend_op::add)
        DECLARE_SIMPLE_PSO_OPTION(rhi::blend_factor, src_color_blend_factor, rhi::blend_factor::one)
        DECLARE_SIMPLE_PSO_OPTION(rhi::blend_factor, dst_color_blend_factor, rhi::blend_factor::zero)

        DECLARE_SIMPLE_PSO_OPTION(rhi::blend_op, alpha_blend_op, rhi::blend_op::add)
        DECLARE_SIMPLE_PSO_OPTION(rhi::blend_factor, src_alpha_blend_factor, rhi::blend_factor::one)
        DECLARE_SIMPLE_PSO_OPTION(rhi::blend_factor, dst_alpha_blend_factor, rhi::blend_factor::zero)

        pso_options& add_color_attachment(const rhi::image_format color_format)
        {
            this->color_formats[this->color_attachments_count++] = color_format;
            return *this;
        }
    };

#undef DECLARE_SIMPLE_PSO_OPTION
}
