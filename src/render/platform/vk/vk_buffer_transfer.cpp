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

    return vk_buffer_transfer {
        .queue = queue, .staging_buffer = *staging_buffer, .staging_command_buffer = *cmd_buffer};
}

void render::destroy_buffer_transfer(VkDevice device, VmaAllocator allocator, const vk_buffer_transfer& buffer_transfer)
{
    render::destroy_buffer(allocator, buffer_transfer.staging_buffer);
    render::destroy_command_buffer(device, buffer_transfer.staging_command_buffer);
}
