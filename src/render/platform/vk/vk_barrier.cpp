#include <render/platform/vk/vk_barrier.hpp>

void render::cmd_stage_barrier_rw(VkCommandBuffer cmd, VkPipelineStageFlags2 src_stage_mask,
                                  VkPipelineStageFlags2 dst_stage_mask)
{
    constexpr VkAccessFlags2 flags = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    cmd_stage_barrier(cmd, src_stage_mask, flags, dst_stage_mask, flags);
}

void render::cmd_stage_barrier_rw(VkCommandBuffer cmd, VkPipelineStageFlags2 joint_stage_mask)
{
    constexpr VkAccessFlags2 flags = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    cmd_stage_barrier(cmd, joint_stage_mask, flags, joint_stage_mask, flags);
}

void render::cmd_stage_barrier(VkCommandBuffer cmd, VkPipelineStageFlags2 src_stage_mask,
                               VkAccessFlags2 src_access_mask, VkPipelineStageFlags2 dst_stage_mask,
                               VkAccessFlags2 dst_access_mask)
{
    const VkMemoryBarrier2 barrier = {
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask  = src_stage_mask,
        .srcAccessMask = src_access_mask,
        .dstStageMask  = dst_stage_mask,
        .dstAccessMask = dst_access_mask,
    };

    const VkDependencyInfo dependency_info = {
        .sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers    = &barrier,
    };

    vkCmdPipelineBarrier2(cmd, &dependency_info);
}
