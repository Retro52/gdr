#pragma once

#include <volk.h>

#include <pod_types.hpp>

namespace render
{
    VkFormat vk_format_from_dxgi(u32 gx_format);
}
