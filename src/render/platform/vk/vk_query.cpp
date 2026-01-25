#include <render/platform/vk/vk_error.hpp>
#include <render/platform/vk/vk_query.hpp>

result<VkQueryPool> render::create_vk_query_pool(VkDevice device, u32 queries, VkQueryType type)
{
    assert(type != VK_QUERY_TYPE_PIPELINE_STATISTICS);

    VkQueryPoolCreateInfo create_info = {
        .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType  = type,
        .queryCount = queries,
    };

    VkQueryPool pool;
    VK_RETURN_ON_FAIL(vkCreateQueryPool(device, &create_info, nullptr, &pool));

    return pool;
}

result<VkQueryPool> render::create_vk_pipeline_stat_query_pool(VkDevice device, u32 queries,
                                                               VkQueryPipelineStatisticFlags flags)
{
    VkQueryPoolCreateInfo create_info = {
        .sType              = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType          = VK_QUERY_TYPE_PIPELINE_STATISTICS,
        .queryCount         = queries,
        .pipelineStatistics = flags,
    };

    VkQueryPool pool;
    VK_RETURN_ON_FAIL(vkCreateQueryPool(device, &create_info, nullptr, &pool));

    return pool;
}
