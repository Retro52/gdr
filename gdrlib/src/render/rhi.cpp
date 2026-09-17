#include <render/rhi.hpp>
#include <render/rhi_d3d12.hpp>
#include <render/rhi_vk.hpp>

render::rhi::rhi render::rhi::create_for_d3d12()
{
    render::rhi::rhi result {
        .create_context    = d3d12_create_context,
        .destroy_context   = d3d12_destroy_context,
        .create_swapchain  = d3d12_create_swapchain,
        .resize_swapchain  = d3d12_resize_swapchain,
        .destroy_swapchain = d3d12_destroy_swapchain,

        .query_shader_stage           = d3d12_query_shader_stage,
        .query_swapchain_images_count = d3d12_query_swapchain_images_count,
        .query_current_frame_index    = d3d12_query_current_frame_index,

        .query_queue           = d3d12_query_queue,
        .query_device          = d3d12_query_device,
        .query_physical_device = d3d12_query_physical_device,
    };
    return result;
}

render::rhi::rhi render::rhi::create_for_vk()
{
    constexpr render::rhi::rhi result {
        .create_context         = vk_create_context,
        .destroy_context        = vk_destroy_context,
        .create_swapchain       = vk_create_swapchain,
        .resize_swapchain       = vk_resize_swapchain,
        .destroy_swapchain      = vk_destroy_swapchain,
        .create_command_buffer  = vk_create_command_buffer,
        .destroy_command_buffer = vk_destroy_command_buffer,

        .create_bindless_set  = vk_create_bindless_set,
        .destroy_bindless_set = vk_destroy_bindless_set,

        .create_shader  = vk_create_shader,
        .destroy_shader = vk_destroy_shader,

        .create_buffer  = vk_create_buffer,
        .destroy_buffer = vk_destroy_buffer,

        .create_compute_pso  = vk_create_compute_pso,
        .create_graphics_pso = vk_create_graphics_pso,
        .destroy_pso         = vk_destroy_pso,

        .query_shader_stage           = vk_query_shader_stage,
        .query_swapchain_images_count = vk_query_swapchain_images_count,
        .query_current_frame_index    = vk_query_current_frame_index,

        .query_queue           = vk_query_queue,
        .query_device          = vk_query_device,
        .query_physical_device = vk_query_physical_device,

        .queue_wait_idle              = vk_queue_wait_idle,
        .device_wait_idle             = vk_device_wait_idle,
        .acquire_next_swapchain_image = vk_acquire_next_swapchain_image,

        .cmd_begin_recording  = vk_cmd_begin_recording,
        .cmd_end_recording    = vk_cmd_end_recording,
        .cmd_transition_image = vk_cmd_transition_image,
        .cmd_present_image    = vk_cmd_present_image,
        .cmd_set_draw_state   = vk_cmd_set_draw_state,
        .cmd_clear_draw_state = vk_cmd_clear_draw_state,
        .cmd_bind_pso         = vk_cmd_bind_pso,
        .cmd_draw_instanced   = vk_cmd_draw_instanced,

    };

    return result;
}
