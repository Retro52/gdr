#pragma once

#include <volk.h>

#include <types.hpp>

#include <result.hpp>

namespace render
{
    struct vk_command_buffer
    {
        VkCommandPool cmd_pool {VK_NULL_HANDLE};
        VkCommandBuffer cmd_buffer {VK_NULL_HANDLE};
    };

    result<vk_command_buffer> create_command_buffer(
        VkDevice device, u32 queue_family,
        VkCommandPoolCreateFlags extra_flags = 0);

    void destroy_command_buffer(VkDevice device, const vk_command_buffer& cmd_buffer);
}
