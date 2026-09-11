#pragma once

#include <cpp/alg_constexpr.hpp>
#include <cpp/containers/heap_array.hpp>
#include <reflection/enum.hpp>
#include <render/platform/vk/vk_image.hpp>
#include <render/types.hpp>
#include <window.hpp>

namespace render
{
    struct vk_swapchain_image
    {
        vk_image image;
        VkSemaphore release_semaphore;
    };

    struct vk_swapchain
    {
        cpp::heap_array<vk_swapchain_image> images;
        VkSurfaceFormatKHR surface_format;

        VkSwapchainKHR sc     = VK_NULL_HANDLE;
        VkFormat depth_format = VK_FORMAT_UNDEFINED;
    };

    struct vk_queue_data
    {
        VkQueue queue = VK_NULL_HANDLE;
        u32 family    = VK_QUEUE_FAMILY_IGNORED;
    };

    using ext_array = cpp::heap_array<const char*>;

    struct vk_context
    {
        vk_queue_data queues[rhi::kQueueTypesCount];

        ext_array instance_extensions;
        ext_array enabled_device_extensions;

        rhi::rendering_features_table enabled_device_features;

        VkInstance instance                      = VK_NULL_HANDLE;
        VkDevice device                          = VK_NULL_HANDLE;
        VkSurfaceKHR surface                     = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device         = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
        VmaAllocator allocator                   = VK_NULL_HANDLE;
    };

    void vk_destroy_swapchain(const vk_context& vk_context, vk_swapchain& swapchain);

    result<vk_swapchain> vk_create_swapchain(const vk_context& vk_context, VkFormat format, ivec2 size,
                                          u32 frames_in_flight, bool vsync,
                                          VkSwapchainKHR old_swapchain = VK_NULL_HANDLE);

    void vk_destroy_context(vk_context& ctx);

    result<vk_context> vk_create_context(const window& window, const rhi::instance_desc& desc);
}
