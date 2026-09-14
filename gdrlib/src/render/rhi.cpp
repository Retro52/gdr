#include <render/rhi.hpp>
#include <render/rhivk.hpp>

render::rhi::rhi render::rhi::create_for_dx12()
{
    render::rhi::rhi result;
    return result;
}

render::rhi::rhi render::rhi::create_for_vk()
{
    const render::rhi::rhi result {
        .create_context         = vk_create_context,
        .destroy_context        = vk_destroy_context,
        .create_swapchain       = vk_create_swapchain,
        .destroy_swapchain      = vk_destroy_swapchain,
        .create_command_buffer  = vk_create_command_buffer,
        .destroy_command_buffer = vk_destroy_command_buffer,

        .create_bindless_set  = vk_create_bindless_set,
        .destroy_bindless_set = vk_destroy_bindless_set,

        .create_shader  = vk_create_shader,
        .destroy_shader = vk_destroy_shader,

        .create_compute_pso  = vk_create_compute_pso,
        .create_graphics_pso = vk_create_graphics_pso,
        .destroy_pso         = vk_destroy_pso,

        .query_shader_stage           = vk_query_shader_stage,
        .query_swapchain_images_count = vk_query_swapchain_images_count,
        .query_current_frame_index    = vk_query_current_frame_index,

        .query_queue           = vk_query_queue,
        .query_device          = vk_query_device,
        .query_physical_device = vk_query_physical_device,

        .device_wait_idle             = vk_device_wait_idle,
        .acquire_next_swapchain_image = vk_acquire_next_swapchain_image,

        .cmd_begin_recording  = vk_cmd_begin_recording,
        .cmd_end_recording    = vk_cmd_end_recording,
        .cmd_transition_image = vk_cmd_transition_image,
        .cmd_present_image    = vk_cmd_present_image,

    };

    return result;
}
