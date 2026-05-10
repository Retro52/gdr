#include <assert2.hpp>
#include <render/platform/vk/vk_buffer_transfer.hpp>

#include <cstring>

result<render::vk_buffer_transfer> render::create_buffer_transfer(VkDevice device, VmaAllocator allocator,
                                                                  const queue_data& queue, u64 staging_memory_size)
{
    const auto cmd_buffer = render::create_command_buffer(device, queue.family, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    if (!cmd_buffer)
    {
        return error(cmd_buffer.message);
    }

    const auto staging_buffer = render::create_buffer(staging_memory_size,
                                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                      allocator,
                                                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    if (!staging_buffer)
    {
        return error(staging_buffer.message);
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

    assert2(region.dstOffset + region.size <= dst.size);
    assert2(region.srcOffset + region.size <= transfer.staging_buffer.size);

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

void render::fill_buffer(const vk_buffer_transfer& transfer, const vk_buffer& dst, const u8* value_ptr, u64 value_size,
                         const VkBufferCopy& region)
{
    assert2((region.size % value_size) == 0);
    const u64 count = region.size / value_size;

    for (u32 i = 0; i < count; i++)
    {
        std::memcpy(static_cast<u8*>(transfer.mapped) + region.srcOffset + value_size * i, value_ptr, value_size);
    }
    submit_transfer(transfer, dst, region);
}

void render::upload_data(const vk_buffer_transfer& transfer, const vk_buffer& dst, const u8* data,
                         const VkBufferCopy& region)
{
    ZoneScoped;

    assert2(data != nullptr);
    assert2(region.size <= transfer.staging_buffer.size - region.srcOffset);
    std::copy_n(data, region.size, (static_cast<u8*>(transfer.mapped)) + region.srcOffset);
    submit_transfer(transfer, dst, region);
}

void render::upload_image(const vk_buffer_transfer& transfer, const vk_image& dst, const u8* data, const u64 data_size,
                          const u32 width, const u32 height, const u32 mips, const u32 block_size,
                          const u32 bits_per_block)
{
    assert2(bits_per_block % 8 == 0);
    auto get_offset = [](const u32 width, const u32 height, const u32 block_size, const u32 bits_per_block)
    {
        if (block_size > 1)
        {
            return ((width + block_size - 1) / block_size) * ((height + block_size - 1) / block_size) * bits_per_block / 8;
        }

        return width * height * bits_per_block / 8;
    };

    VkCommandBufferBeginInfo begin_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    const auto cmd = transfer.staging_command_buffer.cmd_buffer;
    vkBeginCommandBuffer(cmd, &begin_info);

    const VkImageMemoryBarrier2 image_barrier {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext            = nullptr,
        .srcStageMask     = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask    = VK_ACCESS_2_NONE,
        .dstStageMask     = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout        = VK_IMAGE_LAYOUT_GENERAL,
        .image            = dst.image,
        .subresourceRange = image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT),
    };

    const VkDependencyInfo dependency_info {
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                   = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &image_barrier,
    };

    vkCmdPipelineBarrier2(cmd, &dependency_info);

    u32 mip_w      = width;
    u32 mip_h      = height;
    u64 src_offset = 0;

    std::memcpy(transfer.mapped, data, data_size);
    for (unsigned int i = 0; i < mips; ++i)
    {
        VkBufferImageCopy region = {
            src_offset,
            0,
            0,
            {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1},
            {0, 0, 0},
            {mip_w, mip_h, 1},
        };

        vkCmdCopyBufferToImage(cmd, transfer.staging_buffer.buffer, dst.image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
        src_offset += get_offset(mip_w, mip_h, block_size, bits_per_block);

        mip_w = mip_w > 1 ? mip_w / 2 : 1;
        mip_h = mip_h > 1 ? mip_h / 2 : 1;
    }

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit_info {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd,
    };

    vkQueueSubmit(transfer.queue.queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(transfer.queue.queue);
}
