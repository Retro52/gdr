#include <render/platform/vk/vk_error.hpp>
#include <render/platform/vk/vk_timeline_semaphore.hpp>

void platform::vk_destroy_timeline_semaphore(VkDevice device, vk_timeline_semaphore& semaphore)
{
    semaphore.last_value = 0;
    VK_DESTROY(semaphore.semaphore, vkDestroySemaphore, device);
}

auto platform::vk_create_timeline_semaphore(VkDevice device, const u64 initial_value) -> result<vk_timeline_semaphore>
{
    const VkSemaphoreTypeCreateInfo type_info = {
        .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue  = initial_value,
    };

    const VkSemaphoreCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &type_info,
    };

    vk_timeline_semaphore result;
    VK_RETURN_ON_FAIL(vkCreateSemaphore(device, &info, nullptr, &result.semaphore));

    result.last_value = initial_value;
    return result;
}

void platform::vk_wait_on_timeline_semaphore(VkDevice device, const vk_timeline_semaphore& semaphore, u64 value)
{
    const VkSemaphoreWaitInfo info = {
        .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores    = &semaphore.semaphore,
        .pValues        = &value,
    };

    VK_ASSERT_ON_FAIL(vkWaitSemaphores(device, &info, UINT64_MAX));
}
