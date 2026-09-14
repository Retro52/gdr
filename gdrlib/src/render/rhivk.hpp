#pragma once

#include <render/rhi.hpp>

namespace render::rhi
{
    result<context> vk_create_context(const window& window, const instance_desc& desc);
    void vk_destroy_context(context& context);

    result<swapchain> vk_create_swapchain(const context& context, const create_swapchain_info& desc);
    void vk_destroy_swapchain(const context& context, swapchain& swapchain);

    result<command_buffer> vk_create_command_buffer(const context& context, queue_kind queue_kind);
    void vk_destroy_command_buffer(const context& context, command_buffer& cmd);

    result<u32> vk_query_swapchain_images_count(const swapchain& swapchain);
    result<u32> vk_query_current_frame_index(const swapchain& swapchain);

    result<queue> vk_query_queue(const context& context, queue_kind kind);
    result<device> vk_query_device(const context& context);
    result<physical_device> vk_query_physical_device(const context& context);

    void vk_device_wait_idle(const context& context);
    result<image> vk_acquire_next_swapchain_image(const context& context, swapchain& swapchain);

    void vk_cmd_begin_recording(command_buffer& cmd);
    void vk_cmd_end_recording(command_buffer& cmd);
    void vk_cmd_transition_image(command_buffer& cmd, image& dst, image_layout dst_layout);
    void vk_cmd_present_image(command_buffer& cmd, swapchain& swapchain, queue& submit, queue& present);
}
