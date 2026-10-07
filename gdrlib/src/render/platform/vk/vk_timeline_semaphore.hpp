#pragma once

#include <volk.h>

#include <pod_types.hpp>
#include <result.hpp>

namespace render
{
    struct vk_timeline_semaphore
    {
        u64 last_value        = 0;
        VkSemaphore semaphore = VK_NULL_HANDLE;
    };

    void vk_destroy_timeline_semaphore(VkDevice device, vk_timeline_semaphore& semaphore);

    result<vk_timeline_semaphore> vk_create_timeline_semaphore(VkDevice device, u64 initial_value);

    void vk_wait_on_timeline_semaphore(VkDevice device, const vk_timeline_semaphore& semaphore, u64 value);
}
