#pragma once

#include <volk.h>

#include <pod_types.hpp>

namespace render
{
    enum class color_space
    {
        linear,
        srgb
    };

    VkFormat vk_format_from_dxgi(u32 gx_format);

    VkFormat vk_format_force_color_space(VkFormat vk_format, color_space space);
}
