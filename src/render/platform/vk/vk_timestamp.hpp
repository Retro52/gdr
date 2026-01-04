#pragma once

#include <volk.h>

#include <types.hpp>

#include <result.hpp>

namespace render
{
    result<VkQueryPool> create_vk_query_pool(VkDevice device, u32 queries);
}
