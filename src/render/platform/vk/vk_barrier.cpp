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

void render::cmd_buffer_barrier(VkCommandBuffer cmd, VkBuffer buffer, VkPipelineStageFlags2 src_stage,
                                VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access)
{
    VkBufferMemoryBarrier2 buf_barrier = {
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask        = src_stage,
        .srcAccessMask       = src_access,
        .dstStageMask        = dst_stage,
        .dstAccessMask       = dst_access,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = buffer,
        .offset              = 0,
        .size                = VK_WHOLE_SIZE,
    };

    VkDependencyInfo dep_info = {
        .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags          = VK_DEPENDENCY_BY_REGION_BIT,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers    = &buf_barrier,
    };

    vkCmdPipelineBarrier2(cmd, &dep_info);
}
