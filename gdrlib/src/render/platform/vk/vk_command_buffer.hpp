#pragma once

#include <core/containers/local_array.hpp>
#include <pod_types.hpp>
#include <result.hpp>

#include <volk.h>

// clang-format off
#include <tracy/TracyVulkan.hpp>
// clang-format on

namespace platform
{
    struct vk_command_buffer
    {
        VkCommandPool cmd_pool     = VK_NULL_HANDLE;
        VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;

#if TRACY_ENABLE
        core::local_array<std::unique_ptr<tracy::VkCtxScope>> tracy_alive_scopes;
#endif
    };

    result<vk_command_buffer> vk_create_command_buffer(VkDevice device, u32 queue_family,
                                                       VkCommandPoolCreateFlags extra_flags = 0);

    void vk_destroy_command_buffer(VkDevice device, const vk_command_buffer& cmd_buffer);
}
