#include <render/platform/vk/vk_error.hpp>
#include <render/platform/vk/vk_query.hpp>

void render::vk_query::end(VkCommandBuffer cmd, u32 query) const
{
    if (handle)
    {
        vkCmdEndQuery(cmd, handle, query);
    }
}

void render::vk_query::reset(VkCommandBuffer cmd, const u32 first, const u32 count) const
{
    if (handle)
    {
        vkCmdResetQueryPool(cmd, handle, first, count);
    }
}

void render::vk_query::begin(VkCommandBuffer cmd, const u32 query, const u32 flags) const
{
    if (handle)
    {
        vkCmdBeginQuery(cmd, handle, query, flags);
    }
}

result<render::vk_query> render::create_query_pool(VkDevice device, u32 queries, VkQueryType type)
{
    assert2(type != VK_QUERY_TYPE_PIPELINE_STATISTICS);

    VkQueryPoolCreateInfo create_info = {
        .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType  = type,
        .queryCount = queries,
    };

    VkQueryPool pool;
    VK_RETURN_ON_FAIL(vkCreateQueryPool(device, &create_info, nullptr, &pool));

    return vk_query {.handle = pool};
}

result<render::vk_query> render::create_pipeline_stat_query_pool(VkDevice device, u32 queries,
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

    return vk_query {.handle = pool};
}

void render::destroy_query_pool(VkDevice device, vk_query& query)
{
    VK_DESTROY(query.handle, vkDestroyQueryPool, device);
    query.handle = VK_NULL_HANDLE;
}
