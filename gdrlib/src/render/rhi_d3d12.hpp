#pragma once

#include <render/rhi.hpp>

namespace rhi
{
    result<context> d3d12_create_context(const window& window, const instance_desc& desc);
    void d3d12_destroy_context(context& context);

    result<swapchain> d3d12_create_swapchain(context context, const create_swapchain_info& desc);
    result<swapchain> d3d12_resize_swapchain(context context, swapchain swapchain, const create_swapchain_info& desc);
    void d3d12_destroy_swapchain(context context, swapchain& swapchain);

    result<command_buffer> d3d12_create_command_buffer(context context, queue_kind queue_kind);
    void d3d12_destroy_command_buffer(context context, command_buffer& cmd);

    result<bindless_set> d3d12_create_bindless_set(context context, u32 resource_count);
    void d3d12_update_bindless_set(context context, bindless_set set, std::span<const bindless_set_write_info> info);
    void d3d12_destroy_bindless_set(context context, bindless_set& set);

    result<fence> d3d12_create_fence(context context, u64 initial_value);
    void d3d12_destroy_fence(context context, fence& fence);

    result<shader> d3d12_create_shader(context context, const fs::path& path);
    void d3d12_destroy_shader(context context, shader& shader);

    result<buffer> d3d12_create_buffer(context context, const create_buffer_info& buffer_info);
    void d3d12_destroy_buffer(context context, buffer& buffer);

    result<image> d3d12_create_image(context context, const create_image_info& image_info);
    void d3d12_destroy_image(context context, image& image);

    result<image_view> d3d12_create_image_view(context context, image source, const create_image_view_info& view_info);
    void d3d12_destroy_image_view(context context, image_view& view);

    result<sampler> d3d12_create_sampler(context context, const create_sampler_info& sampler_info);
    void d3d12_destroy_sampler(context context, sampler& sampler);

    result<pipeline> d3d12_create_compute_pso(context context, shader shader, std::span<const bindless_set> sets);
    result<pipeline> d3d12_create_graphics_pso(context context, std::span<const shader> shaders,
                                               std::span<const bindless_set> sets, const pso_options& options);
    void d3d12_destroy_pso(context context, pipeline& pso);

    result<shader_stage> d3d12_query_shader_stage(shader shader);
    result<u32> d3d12_query_current_frame_index(swapchain swapchain);
    result<u32> d3d12_query_swapchain_images_count(swapchain swapchain);
    result<image_format> d3d12_query_swapchain_color_format(swapchain swapchain);
    result<bool> d3d12_query_feature_support(context context, feature_flag feature);

    result<queue> d3d12_query_queue(context context, queue_kind kind);
    result<device> d3d12_query_device(context context);
    result<physical_device> d3d12_query_physical_device(context context);

    void d3d12_queue_wait_idle(queue queue);
    void d3d12_device_wait_idle(context context);
    void d3d12_fence_wait_for_value(context context, fence fence, u64 expected_value);

    result<image> d3d12_acquire_next_swapchain_image(context context, swapchain swapchain);

    void d3d12_cmd_begin_recording(command_buffer cmd);
    void d3d12_cmd_end_recording(command_buffer cmd);

    void d3d12_cmd_reset(command_buffer cmd);
    void d3d12_cmd_barriers(command_buffer cmd, const barrier_batch& barriers);
    void d3d12_cmd_image_blit(command_buffer cmd, image src, image dst, const blit_image_info& info);
    void d3d12_cmd_copy_buffer(command_buffer cmd, buffer src_buffer, u64vec2 src_range, buffer dst_buffer,
                               u64 dst_offset);

    void d3d12_cmd_clear_buffer(command_buffer cmd, buffer buffer, u64vec2 range, u32 value);
    void d3d12_cmd_update_buffer(command_buffer cmd, buffer buffer, u64vec2 range, const void* data);
    void d3d12_cmd_copy_buffer_to_image(command_buffer cmd, buffer src, image dst, image_layout layout,
                                        std::span<const copy_image_info> regions);

    void d3d12_cmd_clear_depth_attachment(command_buffer cmd, image image, ds_clear_value value);
    void d3d12_cmd_clear_color_attachment(command_buffer cmd, image image, color_clear_value value);

    void d3d12_cmd_set_draw_state(command_buffer cmd, std::span<const attachment_state_info> color_attachments,
                                  attachment_state_info depth_attachment, uvec4 viewport);
    void d3d12_cmd_clear_draw_state(command_buffer cmd);

    void d3d12_cmd_set_cull_mode(command_buffer cmd, cull_mode mode);
    void d3d12_cmd_set_depth_bias(command_buffer cmd, f32 constant_factor, f32 slope_factor, f32 clamp);

    void d3d12_cmd_bind_pso(command_buffer cmd, pipeline pso);
    void d3d12_cmd_bind_index(command_buffer cmd, buffer index_buffer);

    void d3d12_cmd_dispatch(command_buffer cmd, pipeline pso, uvec3 threads);
    void d3d12_cmd_dispatch_indirect(command_buffer cmd, buffer count_buffer, u32 buffer_offset);

    void d3d12_cmd_draw(command_buffer cmd, u32 vtx_count, u32 instance_count, u32 first_vertex, u32 first_instance);

    void d3d12_cmd_debug_label_begin(command_buffer cmd, context context, const debug_label& label);
    void d3d12_cmd_debug_label_end(command_buffer cmd, context context);

    void d3d12_submit(context context, queue queue, const submit_info& info);
    void d3d12_present(command_buffer cmd, swapchain swapchain, queue submit, queue present);

    result<ffx_context> d3d12_ffx_create_upscale_context(context ctx, const ffx_upscale_desc& desc);
    void d3d12_ffx_destroy_context(ffx_context& ffx_ctx);

    result<vec2> d3d12_ffx_upscale_query_jitter(ffx_context ffx_ctx, u32 render_width, u32 display_width,
                                                i32 jitter_index);
    void d3d12_ffx_upscale_dispatch(ffx_context ffx_ctx, command_buffer cmd, const ffx_upscale_params& params,
                                    const ffx_upscale_resources& resources);
}
