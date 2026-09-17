#pragma once

#include <render/rhi.hpp>

namespace render::rhi
{
    result<context> d3d12_create_context(const window& window, const instance_desc& desc);
    void d3d12_destroy_context(context& context);

    result<swapchain> d3d12_create_swapchain(context context, const create_swapchain_info& desc);
    result<swapchain> d3d12_resize_swapchain(context context, swapchain swapchain, const create_swapchain_info& desc);
    void d3d12_destroy_swapchain(context context, swapchain& swapchain);

    result<command_buffer> d3d12_create_command_buffer(context context, queue_kind queue_kind);
    void d3d12_destroy_command_buffer(context context, command_buffer& cmd);

    result<bindless_set> d3d12_create_bindless_set(context context, u32 resource_count);
    void d3d12_destroy_bindless_set(context context, bindless_set& set);

    result<shader> d3d12_create_shader(context context, const fs::path& path);
    void d3d12_destroy_shader(context context, shader& shader);

    result<buffer> d3d12_create_buffer(context context, const create_buffer_info& buffer_info);
    void d3d12_destroy_buffer(context context, buffer& buffer);

    result<pipeline> d3d12_create_compute_pso(context context, shader shader, std::span<const bindless_set> sets);
    result<pipeline> d3d12_create_graphics_pso(context context, std::span<const shader> shaders,
                                               std::span<const bindless_set> sets, const nlohmann::json& options);
    void d3d12_destroy_pso(context context, pipeline& pso);

    result<VkShaderStageFlagBits> d3d12_query_shader_stage(shader shader);
    result<u32> d3d12_query_swapchain_images_count(swapchain swapchain);
    result<u32> d3d12_query_current_frame_index(swapchain swapchain);

    result<queue> d3d12_query_queue(context context, queue_kind kind);
    result<device> d3d12_query_device(context context);
    result<physical_device> d3d12_query_physical_device(context context);

    void d3d12_queue_wait_idle(queue queue);
    void d3d12_device_wait_idle(context context);
    result<image> d3d12_acquire_next_swapchain_image(context context, swapchain swapchain);

    void d3d12_cmd_begin_recording(command_buffer cmd);
    void d3d12_cmd_end_recording(command_buffer cmd);
    void d3d12_cmd_transition_image(command_buffer cmd, image dst, image_layout dst_layout);
    void d3d12_cmd_present_image(command_buffer cmd, swapchain swapchain, queue submit, queue present);
    void d3d12_cmd_set_draw_state(command_buffer cmd, std::span<const attachment_state_info> color_attachments,
                                  attachment_state_info depth_attachment, uvec4 viewport);
    void d3d12_cmd_clear_draw_state(command_buffer cmd);
    void d3d12_cmd_bind_pso(command_buffer cmd, pipeline pso);
    void d3d12_cmd_draw_instanced(command_buffer cmd, u32 vtx_count, u32 instance_count, u32 first_vertex,
                                  u32 first_instance);

}
