#include <render/platform/vk/vk_buffer_transfer.hpp>

result<render::vk_buffer_transfer> render::create_buffer_transfer(VkDevice device, VmaAllocator allocator,
                                                                  const queue_data& queue, u64 staging_memory_size)
{
    const auto cmd_buffer = render::create_command_buffer(device, queue.family, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    if (!cmd_buffer)
    {
        return cmd_buffer.message;
    }

    const auto staging_buffer = render::create_buffer(staging_memory_size,
                                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                      allocator,
                                                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    if (!staging_buffer)
    {
        return staging_buffer.message;
    }

    void* data = nullptr;
    vmaMapMemory(allocator, staging_buffer->allocation, &data);

    return vk_buffer_transfer {
        .mapped = data, .queue = queue, .staging_buffer = *staging_buffer, .staging_command_buffer = *cmd_buffer};
}

void render::destroy_buffer_transfer(VkDevice device, VmaAllocator allocator, vk_buffer_transfer& buffer_transfer)
{
    buffer_transfer.mapped = nullptr;
    vmaUnmapMemory(allocator, buffer_transfer.staging_buffer.allocation);

    render::destroy_buffer(allocator, buffer_transfer.staging_buffer);
    render::destroy_command_buffer(device, buffer_transfer.staging_command_buffer);
}

void render::submit_transfer(const vk_buffer_transfer& transfer, const vk_buffer& dst, const VkBufferCopy& region)
{
    ZoneScoped;

    assert(region.dstOffset + region.size < dst.size);
    assert(region.srcOffset + region.size < transfer.staging_buffer.size);

    VkCommandBufferBeginInfo begin_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    vkBeginCommandBuffer(transfer.staging_command_buffer.cmd_buffer, &begin_info);
    vkCmdCopyBuffer(transfer.staging_command_buffer.cmd_buffer, transfer.staging_buffer.buffer, dst.buffer, 1, &region);
    vkEndCommandBuffer(transfer.staging_command_buffer.cmd_buffer);

    VkSubmitInfo submit_info {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &transfer.staging_command_buffer.cmd_buffer,
    };

    vkQueueSubmit(transfer.queue.queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(transfer.queue.queue);
}

void render::upload_data(const vk_buffer_transfer& transfer, const vk_buffer& dst, const u8* data,
                         const VkBufferCopy& region)
{
    ZoneScoped;

    assert(data != nullptr);
    std::copy_n(data, region.size, (static_cast<u8*>(transfer.mapped)) + region.srcOffset);
    submit_transfer(transfer, dst, region);
}
