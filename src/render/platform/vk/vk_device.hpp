#pragma once

#include <volk.h>

#include <render/platform/vk/vk_image.hpp>
#include <render/platform/vk/vma.hpp>
#include <result.hpp>
#include <window.hpp>

#include <queue>
#include <string>
#include <vector>

namespace render
{
    using ext_array = std::vector<const char*>;

    struct rendering_features_table
    {
        enum flag
        {
            eValidation       = 1 << 0,
            eDynamicRender    = 1 << 1,
            eSynchronization2 = 1 << 2,
        };

        bool required(rendering_features_table::flag flag) const noexcept
        {
            return (flag & features) > 0;
        }

        rendering_features_table& enable(rendering_features_table::flag flag) noexcept
        {
            features |= flag;
            return *this;
        }

        u32 features {0};
    };

    struct instance_desc
    {
        const char* app_name {""};

        u32 app_version {0};

        ext_array device_extensions;
        rendering_features_table device_features;
    };

    struct swapchain_image
    {
        VkImage image;
        VkImageView image_view;
        VkSemaphore release_semaphore;

        vk_image depth_image;
        VkImageView depth_image_view;
    };

    struct swapchain
    {
        std::vector<swapchain_image> images;
        VkSurfaceFormatKHR surface_format;

        VkSwapchainKHR vk_swapchain {VK_NULL_HANDLE};
        VkFormat depth_format {VK_FORMAT_UNDEFINED};
    };

    struct queue_data
    {
        VkQueue queue {VK_NULL_HANDLE};
        u32 family {VK_QUEUE_FAMILY_IGNORED};
    };

    // Did not want to use enum class cause I would need to constantly cast to the <int> to use it as an array accessor,
    // and default enum has a rather loose naming requirements.
    using queue_kind_t = u32;

    namespace queue_kind
    {
        constexpr queue_kind_t ePresent  = 0;
        constexpr queue_kind_t eTransfer = 1;
        constexpr queue_kind_t eGfx      = 2;
        constexpr queue_kind_t eCompute  = 3;
        constexpr queue_kind_t COUNT     = 4;
    }

    struct context
    {
        rendering_features_table enabled_device_features;

        ext_array instance_extensions;
        ext_array enabled_device_extensions;

        VkInstance instance                      = VK_NULL_HANDLE;
        VkDevice device                          = VK_NULL_HANDLE;
        VkSurfaceKHR surface                     = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device         = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;

        VmaAllocator allocator = VK_NULL_HANDLE;

        queue_data queues[queue_kind::COUNT];
    };

    void destroy_swapchain(const context& vk_context, swapchain& swapchain);

    result<swapchain> create_swapchain(const context& vk_context, VkFormat format, ivec2 size, u32 frames_in_flight,
                                       bool vsync, VkSwapchainKHR old_swapchain = VK_NULL_HANDLE);

    void destroy_context(context& ctx);

    result<context> create_context(const window& window, const instance_desc& desc);
}
