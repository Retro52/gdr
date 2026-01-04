#include <render/platform/vk/vk_error.hpp>
#include <render/platform/vk/vk_timestamp.hpp>

result<VkQueryPool> render::create_vk_query_pool(VkDevice device, u32 queries)
{
    VkQueryPoolCreateInfo create_info = {
        .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType  = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = queries,
    };

    VkQueryPool pool;
    VK_RETURN_ON_FAIL(vkCreateQueryPool(device, &create_info, nullptr, &pool));

    return pool;
}
