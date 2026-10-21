#include <render/rhi.hpp>
#include <render/rhi_d3d12.hpp>
#include <render/rhi_vk.hpp>

#if GDR_ENABLE_DX12_BACKEND
rhi::impl rhi::create_for_d3d12()
{
    constexpr rhi::impl result {
        .create_context         = d3d12_create_context,
        .destroy_context        = d3d12_destroy_context,
        .create_swapchain       = d3d12_create_swapchain,
        .resize_swapchain       = d3d12_resize_swapchain,
        .destroy_swapchain      = d3d12_destroy_swapchain,
        .create_command_buffer  = d3d12_create_command_buffer,
        .destroy_command_buffer = d3d12_destroy_command_buffer,

        .create_bindless_set  = d3d12_create_bindless_set,
        .update_bindless_set  = d3d12_update_bindless_set,
        .destroy_bindless_set = d3d12_destroy_bindless_set,

        .create_fence  = d3d12_create_fence,
        .destroy_fence = d3d12_destroy_fence,

        .create_shader  = d3d12_create_shader,
        .destroy_shader = d3d12_destroy_shader,

        .create_buffer  = d3d12_create_buffer,
        .destroy_buffer = d3d12_destroy_buffer,

        .create_image  = d3d12_create_image,
        .destroy_image = d3d12_destroy_image,

        .create_image_view  = d3d12_create_image_view,
        .destroy_image_view = d3d12_destroy_image_view,

        .create_sampler  = d3d12_create_sampler,
        .destroy_sampler = d3d12_destroy_sampler,

        .create_compute_pso  = d3d12_create_compute_pso,
        .create_graphics_pso = d3d12_create_graphics_pso,
        .destroy_pso         = d3d12_destroy_pso,

        .query_shader_stage           = d3d12_query_shader_stage,
        .query_feature_support        = d3d12_query_feature_support,
        .query_current_frame_index    = d3d12_query_current_frame_index,
        .query_swapchain_images_count = d3d12_query_swapchain_images_count,
        .query_swapchain_color_format = d3d12_query_swapchain_color_format,

        .query_queue           = d3d12_query_queue,
        .query_device          = d3d12_query_device,
        .query_physical_device = d3d12_query_physical_device,

        .queue_wait_idle      = d3d12_queue_wait_idle,
        .device_wait_idle     = d3d12_device_wait_idle,
        .fence_wait_for_value = d3d12_fence_wait_for_value,

        .acquire_next_swapchain_image = d3d12_acquire_next_swapchain_image,

        .cmd_begin_recording = d3d12_cmd_begin_recording,
        .cmd_end_recording   = d3d12_cmd_end_recording,

        .cmd_reset       = d3d12_cmd_reset,
        .cmd_barriers    = d3d12_cmd_barriers,
        .cmd_image_blit  = d3d12_cmd_image_blit,
        .cmd_copy_buffer = d3d12_cmd_copy_buffer,

        .cmd_clear_buffer         = d3d12_cmd_clear_buffer,
        .cmd_update_buffer        = d3d12_cmd_update_buffer,
        .cmd_copy_buffer_to_image = d3d12_cmd_copy_buffer_to_image,

        .cmd_clear_depth_attachment = d3d12_cmd_clear_depth_attachment,
        .cmd_clear_color_attachment = d3d12_cmd_clear_color_attachment,

        .cmd_set_draw_state   = d3d12_cmd_set_draw_state,
        .cmd_clear_draw_state = d3d12_cmd_clear_draw_state,

        .cmd_set_cull_mode  = d3d12_cmd_set_cull_mode,
        .cmd_set_depth_bias = d3d12_cmd_set_depth_bias,

        .cmd_bind_pso   = d3d12_cmd_bind_pso,
        .cmd_bind_index = d3d12_cmd_bind_index,
#if 0
        .cmd_push_bindings     = d3d12_cmd_push_bindings,
        .cmd_push_constants    = d3d12_cmd_push_constants,
        .cmd_push_bindless_set = d3d12_cmd_push_bindless_set,
#endif

        .cmd_dispatch          = d3d12_cmd_dispatch,
        .cmd_dispatch_indirect = d3d12_cmd_dispatch_indirect,

        .cmd_draw = d3d12_cmd_draw,

        .cmd_draw_indexed_indirect       = d3d12_cmd_draw_indexed_indirect,
        .cmd_draw_indexed_indirect_count = d3d12_cmd_draw_indexed_indirect_count,

        .cmd_draw_mesh_indirect       = d3d12_cmd_draw_mesh_indirect,
        .cmd_draw_mesh_indirect_count = d3d12_cmd_draw_mesh_indirect_count,

        .cmd_debug_label_begin = d3d12_cmd_debug_label_begin,
        .cmd_debug_label_end   = d3d12_cmd_debug_label_end,

        .submit  = d3d12_submit,
        .present = d3d12_present,

        .ffx_create_upscale_context = d3d12_ffx_create_upscale_context,
        .ffx_destroy_context        = d3d12_ffx_destroy_context,

        .ffx_upscale_query_jitter = d3d12_ffx_upscale_query_jitter,
        .ffx_upscale_dispatch     = d3d12_ffx_upscale_dispatch,
    };
    return result;
}
#endif

rhi::impl rhi::create_for_vk()
{
    constexpr rhi::impl result {
        .create_context         = vk_create_context,
        .destroy_context        = vk_destroy_context,
        .create_swapchain       = vk_create_swapchain,
        .resize_swapchain       = vk_resize_swapchain,
        .destroy_swapchain      = vk_destroy_swapchain,
        .create_command_buffer  = vk_create_command_buffer,
        .destroy_command_buffer = vk_destroy_command_buffer,

        .create_bindless_set  = vk_create_bindless_set,
        .update_bindless_set  = vk_update_bindless_set,
        .destroy_bindless_set = vk_destroy_bindless_set,

        .create_fence  = vk_create_fence,
        .destroy_fence = vk_destroy_fence,

        .create_shader  = vk_create_shader,
        .destroy_shader = vk_destroy_shader,

        .create_buffer  = vk_create_buffer,
        .destroy_buffer = vk_destroy_buffer,

        .create_image  = vk_create_image,
        .destroy_image = vk_destroy_image,

        .create_image_view  = vk_create_image_view,
        .destroy_image_view = vk_destroy_image_view,

        .create_sampler  = vk_create_sampler,
        .destroy_sampler = vk_destroy_sampler,

        .create_compute_pso  = vk_create_compute_pso,
        .create_graphics_pso = vk_create_graphics_pso,
        .destroy_pso         = vk_destroy_pso,

        .query_shader_stage           = vk_query_shader_stage,
        .query_feature_support        = vk_query_feature_support,
        .query_current_frame_index    = vk_query_current_frame_index,
        .query_swapchain_images_count = vk_query_swapchain_images_count,
        .query_swapchain_color_format = vk_query_swapchain_color_format,

        .query_queue           = vk_query_queue,
        .query_device          = vk_query_device,
        .query_physical_device = vk_query_physical_device,

        .queue_wait_idle      = vk_queue_wait_idle,
        .device_wait_idle     = vk_device_wait_idle,
        .fence_wait_for_value = vk_fence_wait_for_value,

        .acquire_next_swapchain_image = vk_acquire_next_swapchain_image,

        .cmd_begin_recording = vk_cmd_begin_recording,
        .cmd_end_recording   = vk_cmd_end_recording,

        .cmd_reset       = vk_cmd_reset,
        .cmd_barriers    = vk_cmd_barriers,
        .cmd_image_blit  = vk_cmd_image_blit,
        .cmd_copy_buffer = vk_cmd_copy_buffer,

        .cmd_clear_buffer         = vk_cmd_clear_buffer,
        .cmd_update_buffer        = vk_cmd_update_buffer,
        .cmd_copy_buffer_to_image = vk_cmd_copy_buffer_to_image,

        .cmd_clear_depth_attachment = vk_cmd_clear_depth_attachment,
        .cmd_clear_color_attachment = vk_cmd_clear_color_attachment,

        .cmd_set_draw_state   = vk_cmd_set_draw_state,
        .cmd_clear_draw_state = vk_cmd_clear_draw_state,

        .cmd_set_cull_mode  = vk_cmd_set_cull_mode,
        .cmd_set_depth_bias = vk_cmd_set_depth_bias,

        .cmd_bind_pso          = vk_cmd_bind_pso,
        .cmd_bind_index        = vk_cmd_bind_index,
        .cmd_push_bindings     = vk_cmd_push_bindings,
        .cmd_push_constants    = vk_cmd_push_constants,
        .cmd_push_bindless_set = vk_cmd_push_bindless_set,

        .cmd_dispatch          = vk_cmd_dispatch,
        .cmd_dispatch_indirect = vk_cmd_dispatch_indirect,

        .cmd_draw = vk_cmd_draw,

        .cmd_draw_indexed_indirect       = vk_cmd_draw_indexed_indirect,
        .cmd_draw_indexed_indirect_count = vk_cmd_draw_indexed_indirect_count,

        .cmd_draw_mesh_indirect       = vk_cmd_draw_mesh_indirect,
        .cmd_draw_mesh_indirect_count = vk_cmd_draw_mesh_indirect_count,

        .cmd_debug_label_begin = vk_cmd_debug_label_begin,
        .cmd_debug_label_end   = vk_cmd_debug_label_end,

        .submit  = vk_submit,
        .present = vk_present,

        .ffx_create_upscale_context = vk_ffx_create_upscale_context,
        .ffx_destroy_context        = vk_ffx_destroy_context,

        .ffx_upscale_query_jitter = vk_ffx_upscale_query_jitter,
        .ffx_upscale_dispatch     = vk_ffx_upscale_dispatch,
    };

    return result;
}
