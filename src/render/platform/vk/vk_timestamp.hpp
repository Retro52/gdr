#pragma once

#include <render/platform/vk/vk_renderer.hpp>

namespace render
{
    inline VkResult create_vk_query_pool(VkDevice device, u32 queries, VkQueryPool* pool)
    {
        VkQueryPoolCreateInfo create_info = {
            .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType  = VK_QUERY_TYPE_TIMESTAMP,
            .queryCount = queries,
        };

        return vkCreateQueryPool(device, &create_info, nullptr, pool);
    }
}
