#pragma once

#include <render/rhi.hpp>

namespace rhi
{
    result<context> vk_create_context(const window& window, const instance_desc& desc);
    void vk_destroy_context(context& context);

    result<swapchain> vk_create_swapchain(context context, const create_swapchain_info& desc);
    result<swapchain> vk_resize_swapchain(context context, swapchain swapchain, const create_swapchain_info& desc);
    void vk_destroy_swapchain(context context, swapchain& swapchain);

    result<command_buffer> vk_create_command_buffer(context context, queue_kind queue_kind);
    void vk_destroy_command_buffer(context context, command_buffer& cmd);

    result<bindless_set> vk_create_bindless_set(context context, u32 resource_count);
    void vk_update_bindless_set(context context, bindless_set set, std::span<const bindless_set_write_info> info);
    void vk_destroy_bindless_set(context context, bindless_set& set);

    result<fence> vk_create_fence(context context, u64 initial_value);
    void vk_destroy_fence(context context, fence& fence);

    result<shader> vk_create_shader(context context, const fs::path& path);
    void vk_destroy_shader(context context, shader& shader);

    result<buffer> vk_create_buffer(context context, const create_buffer_info& buffer_info);
    void vk_destroy_buffer(context context, buffer& buffer);

    result<image> vk_create_image(context context, const create_image_info& image_info);
    void vk_destroy_image(context context, image& image);

    result<image_view> vk_create_image_view(context context, image source, const create_image_view_info& view_info);
    void vk_destroy_image_view(context context, image_view& view);

    result<sampler> vk_create_sampler(context context, const create_sampler_info& sampler_info);
    void vk_destroy_sampler(context context, sampler& sampler);

    result<pipeline> vk_create_compute_pso(context context, shader shader, std::span<const bindless_set> sets);
    result<pipeline> vk_create_graphics_pso(context context, std::span<const shader> shaders,
                                            std::span<const bindless_set> sets, const pso_options& options);
    void vk_destroy_pso(context context, pipeline& pso);

    result<shader_stage> vk_query_shader_stage(shader shader);
    result<u32> vk_query_current_frame_index(swapchain swapchain);
    result<u32> vk_query_swapchain_images_count(swapchain swapchain);
    result<image_format> vk_query_swapchain_color_format(swapchain swapchain);
    result<bool> vk_query_feature_support(context context, feature_flag feature);

    result<queue> vk_query_queue(context context, queue_kind kind);
    result<device> vk_query_device(context context);
    result<physical_device> vk_query_physical_device(context context);

    void vk_queue_wait_idle(queue queue);
    void vk_device_wait_idle(context context);
    void vk_fence_wait_for_value(context context, fence fence, u64 expected_value);

    result<image> vk_acquire_next_swapchain_image(context context, swapchain swapchain);

    void vk_cmd_begin_recording(command_buffer cmd);
    void vk_cmd_end_recording(command_buffer cmd);

    void vk_cmd_reset(command_buffer cmd);
    void vk_cmd_barriers(command_buffer cmd, const barrier_batch& barriers);
    void vk_cmd_image_blit(command_buffer cmd, image src, image dst, const blit_image_info& info);
    void vk_cmd_copy_buffer(command_buffer cmd, buffer src_buffer, u64vec2 src_range, buffer dst_buffer,
                            u64 dst_offset);

    void vk_cmd_clear_buffer(command_buffer cmd, buffer buffer, u64vec2 range, u32 value);
    void vk_cmd_update_buffer(command_buffer cmd, buffer buffer, u64vec2 range, const void* data);
    void vk_cmd_copy_buffer_to_image(command_buffer cmd, buffer src, image dst, image_layout layout,
                                     std::span<const copy_image_info> regions);

    void vk_cmd_clear_depth_attachment(command_buffer cmd, image image, ds_clear_value value);
    void vk_cmd_clear_color_attachment(command_buffer cmd, image image, color_clear_value value);

    void vk_cmd_set_draw_state(command_buffer cmd, std::span<const attachment_state_info> color_attachments,
                               attachment_state_info depth_attachment, uvec4 viewport);
    void vk_cmd_clear_draw_state(command_buffer cmd);

    void vk_cmd_set_cull_mode(command_buffer cmd, cull_mode mode);
    void vk_cmd_set_depth_bias(command_buffer cmd, f32 constant_factor, f32 slope_factor, f32 clamp);

    void vk_cmd_bind_pso(command_buffer cmd, pipeline pso);
    void vk_cmd_bind_index(command_buffer cmd, buffer index_buffer);
    void vk_cmd_push_constants(command_buffer cmd, pipeline pso, const void* data, u32 size, u32 offset);
    void vk_cmd_push_bindings(command_buffer cmd, pipeline pso, std::span<const binding> bindings);
    void vk_cmd_push_bindless_set(command_buffer cmd, pipeline pso, bindless_set set, u32 binding);

    void vk_cmd_dispatch(command_buffer cmd, pipeline pso, uvec3 threads);
    void vk_cmd_dispatch_indirect(command_buffer cmd, buffer count_buffer, u32 buffer_offset);

    void vk_cmd_draw(command_buffer cmd, u32 vtx_count, u32 instance_count, u32 first_vertex, u32 first_instance);

    void vk_cmd_draw_indexed_indirect(command_buffer cmd, buffer buffer, u64 offset, u32 count, u32 stride);

    void vk_cmd_draw_indexed_indirect_count(command_buffer cmd, rhi::buffer buffer, u64 offset,
                                            rhi::buffer count_buffer, u64 count_offset, u32 max_count, u32 stride);

    void vk_cmd_draw_mesh_indirect(command_buffer cmd, buffer buffer, u64 offset, u32 count, u32 stride);

    void vk_cmd_draw_mesh_indirect_count(command_buffer cmd, rhi::buffer buffer, u64 offset, rhi::buffer count_buffer,
                                         u64 count_offset, u32 max_count, u32 stride);

    void vk_submit(context context, queue queue, const submit_info& info);
    void vk_present(command_buffer cmd, swapchain swapchain, queue submit, queue present);
}
