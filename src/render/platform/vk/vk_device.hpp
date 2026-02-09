#pragma once

#include <cpp/alg_constexpr.hpp>
#include <render/platform/vk/vk_image.hpp>
#include <render/platform/vk/vma.hpp>
#include <result.hpp>
#include <window.hpp>

#include <vector>

namespace render
{
    using ext_array = std::vector<const char*>;

    struct rendering_features_table
    {
        enum flag : u32
        {
            eValidation       = 1 << 0,
            eMeshShading      = 1 << 1,
            eDynamicRender    = 1 << 2,
            eSynchronization2 = 1 << 3,
            eDrawIndirect     = 1 << 4,
            e8BitIntegers     = 1 << 5,
            ePipelineStats    = 1 << 6,
            eSamplerMinMax    = 1 << 7,
            eCOUNT
        };

        [[nodiscard]] bool required(rendering_features_table::flag flag) const noexcept
        {
            return (flag & required_features) > 0;
        }

        [[nodiscard]] bool requested(rendering_features_table::flag flag) const noexcept
        {
            return (flag & requested_features) > 0 || required(flag);
        }

        [[nodiscard]] bool supported(rendering_features_table::flag flag) const noexcept
        {
            return (flag & supported_features) > 0;
        }

        [[nodiscard]] bool wanted(rendering_features_table::flag flag) const noexcept
        {
            return (flag & supported_features) > 0 && requested(flag);
        }

        rendering_features_table& require(rendering_features_table::flag flag) noexcept
        {
            required_features |= flag;
            return *this;
        }

        rendering_features_table& request(rendering_features_table::flag flag) noexcept
        {
            requested_features |= flag;
            return *this;
        }

        void set_supported(rendering_features_table::flag flag, bool supported = true) noexcept
        {
            supported_features = supported ? supported_features | flag : supported_features & ~flag;
        }

        [[nodiscard]] bool all_required_supported() const noexcept
        {
            bool result = true;
            for (u32 i = 0; i < cpp::cx_get_enum_bit_count(rendering_features_table::eCOUNT); ++i)
            {
                const auto feature = static_cast<flag>(1 << i);
                result |= cpp::cx_implies(this->required(feature), this->requested(feature));
            }

            return result;
        }

        u32 required_features {0};
        u32 requested_features {0};
        u32 supported_features {0};
    };

    struct instance_desc
    {
        const char* app_name {""};
        u32 app_version {0};
        rendering_features_table device_features;
    };

    struct swapchain_image
    {
        VkImage image;
        VkImageView image_view;
        VkSemaphore release_semaphore;
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
