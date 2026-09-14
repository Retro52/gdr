#include <render/platform/vk/vk_error.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <tracy/Tracy.hpp>

using namespace render;

vk_renderer::vk_renderer(const render::instance_desc& desc, const window& window, bool vsync)
    : m_context(*render::create_context(window, desc))
{
    ZoneScoped;

    recreate_swapchain(window.get_size_in_px(), vsync);
    m_in_flight_frames.resize(m_swapchain.images.size());

    const VkSemaphoreCreateInfo semaphore_create_info {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    const VkFenceCreateInfo fence_create_info {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (auto& frame : m_in_flight_frames)
    {
        frame.command_buffer =
            *render::create_command_buffer(m_context.device, m_context.queues[render::queue_kind::eGfx].family);

        VK_ASSERT_ON_FAIL(vkCreateFence(m_context.device, &fence_create_info, nullptr, &frame.fence));
        VK_ASSERT_ON_FAIL(
            vkCreateSemaphore(m_context.device, &semaphore_create_info, nullptr, &frame.acquire_semaphore));
        TRACY_ONLY(frame.tracy_ctx = TracyVkContextCalibrated(m_context.physical_device,
                                                              m_context.device,
                                                              m_context.queues[render::queue_kind::eGfx].queue,
                                                              frame.command_buffer.cmd_buffer,
                                                              vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
                                                              vkGetCalibratedTimestampsEXT))
    }
}

vk_renderer::~vk_renderer()
{
    ZoneScoped;
    for (auto& frame : m_in_flight_frames)
    {
        flush_delete_callbacks(frame.delete_callbacks);

        vkDestroyFence(m_context.device, frame.fence, nullptr);
        vkDestroySemaphore(m_context.device, frame.acquire_semaphore, nullptr);

#if TRACY_ENABLE
        TracyVkDestroy(frame.tracy_ctx);
#endif

        render::destroy_command_buffer(m_context.device, frame.command_buffer);
    }

    render::destroy_swapchain(m_context, m_swapchain);
    render::destroy_context(m_context);
}

[[nodiscard]] const render::context& vk_renderer::get_context() const
{
    return m_context;
}

[[nodiscard]] const render::swapchain& vk_renderer::get_swapchain() const
{
    return m_swapchain;
}

void vk_renderer::resize_swapchain(ivec2 new_size)
{
    ZoneScoped;
    recreate_swapchain(new_size, get_vsync());
}

[[nodiscard]] bool vk_renderer::acquire_frame()
{
    ZoneScoped;
    vkWaitForFences(m_context.device, 1, &m_in_flight_frames[m_frame_index].fence, VK_TRUE, UINT64_MAX);

    u32 new_image_index       = 0;
    const auto acquire_result = vkAcquireNextImageKHR(m_context.device,
                                                      m_swapchain.vk_swapchain,
                                                      UINT64_MAX,
                                                      m_in_flight_frames[m_frame_index].acquire_semaphore,
                                                      VK_NULL_HANDLE,
                                                      &new_image_index);
    flush_delete_callbacks(m_frame_index);
    m_image_index = new_image_index & 0xFF;

    switch (acquire_result)
    {
    case VK_SUCCESS :
    case VK_SUBOPTIMAL_KHR :
        vkResetFences(m_context.device, 1, &m_in_flight_frames[m_frame_index].fence);
        return true;
    case VK_ERROR_OUT_OF_DATE_KHR :
        resize_swapchain(m_swapchain_size);
        [[fallthrough]];
    default :
        return false;
    }
}

void vk_renderer::present_frame(VkCommandBuffer buffer)
{
    ZoneScoped;
    const VkSemaphoreSubmitInfo wait_semaphore_info {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext     = nullptr,
        .semaphore = m_in_flight_frames[m_frame_index].acquire_semaphore,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    const VkSemaphoreSubmitInfo signal_semaphore_info {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext     = nullptr,
        .semaphore = m_swapchain.images[m_image_index].release_semaphore,
    };

    const VkCommandBufferSubmitInfo command_buffer_submit_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .pNext = nullptr, .commandBuffer = buffer};

    const VkSubmitInfo2 gfx_submit_info {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,

        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos    = &wait_semaphore_info,

        .commandBufferInfoCount = 1,
        .pCommandBufferInfos    = &command_buffer_submit_info,

        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos    = &signal_semaphore_info,
    };

    VK_ASSERT_ON_FAIL(vkQueueSubmit2(m_context.queues[render::queue_kind::eGfx].queue,
                                     1,
                                     &gfx_submit_info,
                                     m_in_flight_frames[m_frame_index].fence));

    u32 img_index = m_image_index;
    const VkPresentInfoKHR present_info_khr {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &m_swapchain.images[img_index].release_semaphore,
        .swapchainCount     = 1,
        .pSwapchains        = &m_swapchain.vk_swapchain,
        .pImageIndices      = &img_index,
        .pResults           = nullptr,
    };

    const auto present_result =
        vkQueuePresentKHR(m_context.queues[render::queue_kind::ePresent].queue, &present_info_khr);

    switch (present_result)
    {
    case VK_SUBOPTIMAL_KHR :
    case VK_ERROR_OUT_OF_DATE_KHR :
        resize_swapchain(m_swapchain_size);
        return;
    default :
        VK_ASSERT_ON_FAIL(present_result);
    }

    m_frame_index = (m_frame_index.value() + 1) % m_in_flight_frames.size();
}

u8 vk_renderer::get_frame_index() const
{
    return m_frame_index;
}

u8 vk_renderer::get_frames_in_flight() const
{
    return m_in_flight_frames.size();
}

[[nodiscard]] VkRect2D vk_renderer::get_scissor() const
{
    return {
        .offset = {0,                                    0                                   },
        .extent = {static_cast<u32>(m_swapchain_size.x), static_cast<u32>(m_swapchain_size.y)},
    };
}

[[nodiscard]] VkViewport vk_renderer::get_viewport() const
{
    return {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = static_cast<float>(m_swapchain_size.x),
        .height   = static_cast<float>(m_swapchain_size.y),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
}

#if TRACY_ENABLE
[[nodiscard]] TracyVkCtx vk_renderer::get_frame_tracy_context() const
{
    return m_in_flight_frames[m_frame_index].tracy_ctx;
}
#endif

[[nodiscard]] VkCommandBuffer vk_renderer::get_frame_command_buffer() const
{
    return m_in_flight_frames[m_frame_index].command_buffer.cmd_buffer;
}

[[nodiscard]] render::swapchain_image vk_renderer::get_frame_swapchain_image() const
{
    return m_swapchain.images[m_image_index];
}

[[nodiscard]] cpp::heap_array<vk_renderer::delete_callback_t>& vk_renderer::get_frame_callbacks_queue()
{
    return m_in_flight_frames[m_frame_index].delete_callbacks;
}

void vk_renderer::set_vsync(bool vsync)
{
    recreate_swapchain(m_swapchain_size, vsync);
}

[[nodiscard]] bool vk_renderer::get_vsync() const
{
    return m_frame_index.get_flag(tagged_bits::vsync_bit);
}

[[nodiscard]] bool vk_renderer::is_feature_supported(feature_flag feature) const
{
    return m_context.enabled_device_features.supported(feature);
}

void vk_renderer::flush_delete_callbacks(u32 frame_index)
{
    flush_delete_callbacks(m_in_flight_frames[frame_index].delete_callbacks);
}

void vk_renderer::flush_delete_callbacks(cpp::heap_array<vk_renderer::delete_callback_t>& callbacks)
{
    for (auto& callback : callbacks)
    {
        std::invoke(callback, m_context.device, m_context.allocator);
    }

    callbacks.clear();
}

void vk_renderer::recreate_swapchain(ivec2 new_size, bool vsync)
{
    if ((new_size == m_swapchain_size || new_size.x < 1 || new_size.y < 1) && vsync == get_vsync())
    {
        return;
    }

    force_recreate_swapchain(new_size, vsync);
}

void vk_renderer::force_recreate_swapchain(ivec2 new_size, bool vsync)
{
    if (const auto created_sc = render::create_swapchain(
            m_context, VK_FORMAT_B8G8R8A8_UNORM, new_size, kFramesInFlight, vsync, m_swapchain.vk_swapchain))
    {
        render::destroy_swapchain(m_context, m_swapchain);

        m_swapchain_size = new_size;
        m_swapchain      = *created_sc;
        m_frame_index.set_flag(tagged_bits::vsync_bit, vsync);
    }
}
