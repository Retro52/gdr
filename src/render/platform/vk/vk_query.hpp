#pragma once

#include <volk.h>

#include <types.hpp>

#include <result.hpp>

namespace render
{
    result<VkQueryPool> create_vk_query_pool(VkDevice device, u32 queries, VkQueryType type);
    result<VkQueryPool> create_vk_pipeline_stat_query_pool(VkDevice device, u32 queries, VkQueryPipelineStatisticFlags flags);
}
