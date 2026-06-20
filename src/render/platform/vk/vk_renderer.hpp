#pragma once

#include <cpp/containers/heap_array.hpp>
#include <cpp/tagged_int.hpp>
#include <render/platform/vk/vk_command_buffer.hpp>
#include <render/platform/vk/vk_device.hpp>
#include <tracy/TracyVulkan.hpp>
#include <window.hpp>

namespace render
{
    class vk_renderer
    {
    private:
        struct frame_data
        {
            TRACY_ONLY(TracyVkCtx tracy_ctx {});
            vk_command_buffer command_buffer;

            VkFence fence {VK_NULL_HANDLE};
            VkSemaphore acquire_semaphore {VK_NULL_HANDLE};
        };

    public:
        vk_renderer(const render::instance_desc& desc, const window& window, bool vsync);

        ~vk_renderer();

        [[nodiscard]] const render::context& get_context() const;

        [[nodiscard]] const render::swapchain& get_swapchain() const;

        void resize_swapchain(ivec2 new_size);

        [[nodiscard]] bool acquire_frame();

        void present_frame(VkCommandBuffer buffer);

        [[nodiscard]] u8 get_frame_index() const;

        [[nodiscard]] u8 get_frames_in_flight() const;

        [[nodiscard]] VkRect2D get_scissor() const;

        [[nodiscard]] VkViewport get_viewport() const;

#if TRACY_ENABLE
        [[nodiscard]] TracyVkCtx get_frame_tracy_context() const;
#endif

        [[nodiscard]] VkCommandBuffer get_frame_command_buffer() const;

        [[nodiscard]] render::swapchain_image get_frame_swapchain_image() const;

        void set_vsync(bool vsync);

        [[nodiscard]] bool get_vsync() const;

        [[nodiscard]] bool is_feature_supported(feature_flag feature) const;

        template<typename Func>
        void submit(Func&& func) const
        {
            std::invoke(func, get_frame_command_buffer());
        }

    private:
        void recreate_swapchain(ivec2 new_size, bool vsync);

        void force_recreate_swapchain(ivec2 new_size, bool vsync);

    private:
        constexpr static u32 kFramesInFlight = 2;

        enum tagged_bits
        {
            vsync_bit,
            count
        };

        render::context m_context;
        render::swapchain m_swapchain;

        cpp::heap_array<frame_data> m_in_flight_frames;

        ivec2 m_swapchain_size {};

        cpp::tagged_int<u8, tagged_bits::count> m_frame_index {0};
        u8 m_image_index {0};
    };
}
