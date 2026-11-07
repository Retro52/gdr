#pragma once

#include <core/alg_constexpr.hpp>
#include <core/containers/heap_array.hpp>
#include <reflection/enum.hpp>
#include <render/platform/vk/vk_image.hpp>
#include <render/types.hpp>
#include <tracy/TracyVulkan.hpp>
#include <window.hpp>

namespace platform
{
    struct vk_swapchain_image
    {
        vk_image image;
        VkSemaphore acquire_semaphore = VK_NULL_HANDLE;
        VkSemaphore release_semaphore = VK_NULL_HANDLE;
    };

    struct vk_swapchain
    {
        core::heap_array<vk_swapchain_image> images;
        VkSurfaceFormatKHR surface_format;

        VkSwapchainKHR sc = VK_NULL_HANDLE;
    };

    struct vk_queue_data
    {
        VkQueue queue = VK_NULL_HANDLE;
        u32 family    = VK_QUEUE_FAMILY_IGNORED;
    };

    struct vk_context
    {
        vk_queue_data queues[rhi::kQueueTypesCount];

        core::heap_array<const char*> instance_extensions;
        rhi::rendering_features_table enabled_device_features;

        VkInstance instance                      = VK_NULL_HANDLE;
        VkDevice device                          = VK_NULL_HANDLE;
        VkSurfaceKHR surface                     = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device         = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
        VmaAllocator allocator                   = VK_NULL_HANDLE;
#if TRACY_ENABLE
        TracyVkCtx tracy_ctx = {};
#endif
    };

    void vk_destroy_swapchain(const vk_context& vk_context, vk_swapchain& swapchain);

    result<vk_swapchain> vk_create_swapchain(const vk_context& vk_context, VkFormat format, ivec2 size,
                                             u32 request_images_count, bool vsync,
                                             const vk_swapchain* old_swapchain = nullptr);

    void vk_destroy_context(vk_context& ctx);

    result<vk_context> vk_create_context(const window& window, const rhi::instance_desc& desc);
}
