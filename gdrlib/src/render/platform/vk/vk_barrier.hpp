#pragma once

#include <volk.h>

namespace render
{
    void cmd_stage_barrier_rw(VkCommandBuffer cmd, VkPipelineStageFlags2 joint_stage_mask);

    void cmd_stage_barrier_rw(VkCommandBuffer cmd, VkPipelineStageFlags2 src_stage_mask,
                              VkPipelineStageFlags2 dst_stage_mask);

    void cmd_stage_barrier(VkCommandBuffer cmd, VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask,
                           VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask);

    void cmd_buffer_barrier(VkCommandBuffer cmd, VkBuffer buffer, VkPipelineStageFlags2 src_stage,
                            VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access);
}
