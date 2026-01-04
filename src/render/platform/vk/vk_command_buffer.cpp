#include <render/platform/vk/vk_command_buffer.hpp>
#include <render/platform/vk/vk_error.hpp>
#include <Tracy/Tracy.hpp>

result<render::vk_command_buffer> render::create_command_buffer(VkDevice device, u32 queue_family,
                                                                VkCommandPoolCreateFlags extra_flags)
{
    ZoneScoped;
    vk_command_buffer buffer;

    extra_flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    const VkCommandPoolCreateInfo cmd_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = extra_flags, .queueFamilyIndex = queue_family};

    VK_RETURN_ON_FAIL(vkCreateCommandPool(device, &cmd_pool_create_info, nullptr, &buffer.cmd_pool));

    const VkCommandBufferAllocateInfo cmd_buffer_allocate_info = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = buffer.cmd_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_RETURN_ON_FAIL(vkAllocateCommandBuffers(device, &cmd_buffer_allocate_info, &buffer.cmd_buffer));

    return buffer;
}

void render::destroy_command_buffer(VkDevice device, const vk_command_buffer& cmd_buffer)
{
    vkFreeCommandBuffers(device, cmd_buffer.cmd_pool, 1, &cmd_buffer.cmd_buffer);
    vkDestroyCommandPool(device, cmd_buffer.cmd_pool, nullptr);
}
