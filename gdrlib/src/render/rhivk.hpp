#pragma once

#include <render/rhi.hpp>

namespace render::rhi
{
    result<context> vk_create_context(const window& window, const instance_desc& desc);
    void vk_destroy_context(context& context);

    result<swapchain> vk_create_swapchain(context context, const create_swapchain_info& desc);
    void vk_destroy_swapchain(context context, swapchain& swapchain);

    result<command_buffer> vk_create_command_buffer(context context, queue_kind queue_kind);
    void vk_destroy_command_buffer(context context, command_buffer& cmd);

    result<bindless_set> vk_create_bindless_set(context context, u32 resource_count);
    void vk_destroy_bindless_set(context context, bindless_set& set);

    result<shader> vk_create_shader(context context, const fs::path& path);
    void vk_destroy_shader(context context, shader& shader);

    result<pipeline> vk_create_compute_pso(context context, shader shader, std::span<const bindless_set> sets);
    result<pipeline> vk_create_graphics_pso(context context, std::span<const shader> shaders,
                                            std::span<const bindless_set> sets, const nlohmann::json& options);
    void vk_destroy_pso(context context, pipeline& pso);

    result<VkShaderStageFlagBits> vk_query_shader_stage(shader shader);
    result<u32> vk_query_swapchain_images_count(swapchain swapchain);
    result<u32> vk_query_current_frame_index(swapchain swapchain);

    result<queue> vk_query_queue(context context, queue_kind kind);
    result<device> vk_query_device(context context);
    result<physical_device> vk_query_physical_device(context context);

    void vk_device_wait_idle(context context);
    result<image> vk_acquire_next_swapchain_image(context context, swapchain swapchain);

    void vk_cmd_begin_recording(command_buffer cmd);
    void vk_cmd_end_recording(command_buffer cmd);
    void vk_cmd_transition_image(command_buffer cmd, image dst, image_layout dst_layout);
    void vk_cmd_present_image(command_buffer cmd, swapchain swapchain, queue submit, queue present);
}
