#pragma once

#include <volk.h>

#include <pod_types.hpp>
#include <result.hpp>

namespace render
{
    struct vk_query
    {
        VkQueryPool handle {VK_NULL_HANDLE};

        void end(VkCommandBuffer cmd, u32 query) const;

        void reset(VkCommandBuffer cmd, u32 first, u32 count) const;

        void begin(VkCommandBuffer cmd, u32 query, u32 flags) const;
    };

    result<vk_query> create_query_pool(VkDevice device, u32 queries, VkQueryType type);
    result<vk_query> create_pipeline_stat_query_pool(VkDevice device, u32 queries, VkQueryPipelineStatisticFlags flags);

    void destroy_query_pool(VkDevice device, vk_query& query);
}
