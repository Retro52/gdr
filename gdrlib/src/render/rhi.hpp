#pragma once

#include <render/resources.hpp>
#include <render/types.hpp>
#include <result.hpp>
#include <window.hpp>

namespace render::rhi
{
    using destroy_context_fpn = void (*)(context& context);
    using create_context_fpn  = result<context> (*)(const window& window, const instance_desc& desc);

    using destroy_swapchain_fpn = void (*)(const context& context, swapchain& swapchain);
    using create_swapchain_fpn  = result<swapchain> (*)(const context& context, const create_swapchain_info& desc);

    using create_command_buffer_fpn  = result<command_buffer> (*)(const context& context, queue_kind queue_kind);
    using destroy_command_buffer_fpn = void (*)(const context& context, command_buffer& cmd);

    using query_swapchain_images_count_fpn = result<u32> (*)(const swapchain& swapchain);
    using query_current_frame_index_fpn    = result<u32> (*)(const swapchain& swapchain);

    using query_queue_fpn           = result<queue> (*)(const context& context, queue_kind kind);
    using query_device_fpn          = result<device> (*)(const context& context);
    using query_physical_device_fpn = result<physical_device> (*)(const context& context);

    using device_wait_idle_fpn             = void (*)(const context& context);
    using acquire_next_swapchain_image_fpn = result<image> (*)(const context& context, swapchain& swapchain);

    using cmd_begin_recording_fpn  = void (*)(command_buffer& cmd);
    using cmd_end_recording_fpn    = void (*)(command_buffer& cmd);
    using cmd_transition_image_fpn = void (*)(command_buffer& cmd, image& dst, image_layout dst_layout);
    using cmd_present_image_fpn    = void (*)(command_buffer& cmd, swapchain& swapchain, queue& submit, queue& present);

    struct rhi
    {
        create_context_fpn create_context;
        destroy_context_fpn destroy_context;

        create_swapchain_fpn create_swapchain;
        destroy_swapchain_fpn destroy_swapchain;

        create_command_buffer_fpn create_command_buffer;
        destroy_command_buffer_fpn destroy_command_buffer;

        query_swapchain_images_count_fpn query_swapchain_images_count;
        query_current_frame_index_fpn query_current_frame_index;

        query_queue_fpn query_queue;
        query_device_fpn query_device;
        query_physical_device_fpn query_physical_device;

        device_wait_idle_fpn device_wait_idle;
        acquire_next_swapchain_image_fpn acquire_next_swapchain_image;

        cmd_begin_recording_fpn cmd_begin_recording;
        cmd_end_recording_fpn cmd_end_recording;
        cmd_transition_image_fpn cmd_transition_image;
        cmd_present_image_fpn cmd_present_image;
    };

    rhi create_for_vk();

    rhi create_for_dx12();
}
