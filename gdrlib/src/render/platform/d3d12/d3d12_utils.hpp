#pragma once

#include <volk.h>

#include <pod_types.hpp>

namespace render
{
    u32 d3d12_format_from_vk(VkFormat vk_format);
}
