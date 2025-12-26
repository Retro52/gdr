#pragma once

#include <render/platform/vk/vk_device.hpp>
#include <window.hpp>

namespace render
{
    class vk_renderer
    {
    public:
        struct command_buffer
        {
            VkCommandPool vk_pool {VK_NULL_HANDLE};
            VkCommandBuffer vk_buffer {VK_NULL_HANDLE};
        };

    private:
        struct frame_data
        {
            VkFence fence {VK_NULL_HANDLE};
            VkSemaphore acquire_semaphore {VK_NULL_HANDLE};

            vk_renderer::command_buffer command_buffer;
        };

    public:
        vk_renderer(const render::instance_desc& desc, const window& window);

        [[nodiscard]] const render::context& get_context() const;

        [[nodiscard]] const render::swapchain& get_swapchain() const;

        void resize_swapchain(ivec2 new_size);

        [[nodiscard]] result<command_buffer> create_command_buffer() const;

        [[nodiscard]] bool acquire_frame();

        void present_frame(VkCommandBuffer buffer);

        [[nodiscard]] u32 get_frame_index() const;

        [[nodiscard]] u32 get_frames_in_flight() const;

        [[nodiscard]] VkRect2D get_scissor() const;

        [[nodiscard]] VkViewport get_viewport() const;

        [[nodiscard]] VkCommandBuffer get_frame_command_buffer() const;

        [[nodiscard]] render::swapchain_image get_frame_swapchain_image() const;

        [[nodiscard]] VkShaderModule create_shader_module(const bytes& binary) const;

        template<typename Func>
        void submit(Func&& func) const
        {
            std::invoke(func, get_frame_command_buffer());
        }

    private:
        constexpr static bool kUseVsync      = false;
        constexpr static u32 kFramesInFlight = 3;

    private:
        render::context m_context;
        render::swapchain m_swapchain;

        std::vector<frame_data> m_in_flight_frames;

        ivec2 m_swapchain_size {};

        u32 m_frame_index {0};
        u32 m_image_index {0};
    };
}
